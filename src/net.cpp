#include "net.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using sock_t = SOCKET;
static void close_sock(sock_t s) { closesocket(s); }
static bool bad(sock_t s) { return s == INVALID_SOCKET; }
struct WsaInit { WsaInit() { WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); } } wsa_init;
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using sock_t = int;
static void close_sock(sock_t s) { ::close(s); }
static bool bad(sock_t s) { return s < 0; }
#endif

namespace {
int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool send_all(sock_t s, const std::string& data) {
  size_t off = 0;
  while (off < data.size()) {
    int n = ::send(s, data.data() + off, int(data.size() - off), 0);
    if (n <= 0) return false;
    off += size_t(n);
  }
  return true;
}

// Reads lines from a socket until it closes; each line goes to on_line.
void read_lines(sock_t s, const std::function<bool(const std::string&)>& on_line) {
  std::string buf;
  char tmp[4096];
  while (true) {
    int n = ::recv(s, tmp, sizeof tmp, 0);
    if (n <= 0) return;
    buf.append(tmp, size_t(n));
    size_t nl;
    while ((nl = buf.find('\n')) != std::string::npos) {
      std::string line = buf.substr(0, nl);
      buf.erase(0, nl + 1);
      if (!on_line(line)) return;
    }
    if (buf.size() > 1 << 20) return;  // no line is that long
  }
}

class Queue {
 public:
  void push(json j) { std::lock_guard<std::mutex> l(m_); q_.push_back(std::move(j)); }
  std::optional<json> pop() {
    std::lock_guard<std::mutex> l(m_);
    if (q_.empty()) return std::nullopt;
    json j = std::move(q_.front());
    q_.pop_front();
    return j;
  }
 private:
  std::mutex m_;
  std::deque<json> q_;
};
}  // namespace

struct Host::Peer {
  std::function<void(const std::string&)> out;
  int pid = 0, floor = -1, version = -1;
  std::atomic<bool> dead{false};
};

static json journal_head(const std::string& path) {
  std::ifstream f(path);
  std::string line;
  if (!std::getline(f, line)) return json();
  json head = json::parse(line, nullptr, false);
  return head.is_object() ? head : json();
}

static uint64_t journal_seed(const std::string& path) {
  std::ifstream f(path);
  std::string line;
  if (!std::getline(f, line)) return 0;
  json head = json::parse(line, nullptr, false);
  if (!head.is_object() || head.value("build", "") != PYREHEART_BUILD) return 0;
  return head.value("seed", uint64_t(0));
}

bool saved_dive(const std::string& journal) { return journal_seed(journal) != 0; }

Host::Host(uint64_t seed, bool open, const std::string& journal, bool resume, int frost)
    : game_(resume && journal_seed(journal) ? journal_seed(journal) : seed,
            resume && journal_seed(journal) ? journal_head(journal).value("frost", 0) : frost),
      journal_(journal) {
  if (resume && journal_seed(journal)) {
    std::ifstream f(journal);
    std::string line;
    std::getline(f, line);
    while (std::getline(f, line)) {
      json e = json::parse(line, nullptr, false);
      if (!e.is_object()) break;  // a line cut short when the app was killed
      if (e.contains("j")) {
        game_.join(e["j"][0], e["j"][1], e["j"][2], e["j"].size() > 3 ? e["j"][3] : json(), e["j"].size() > 4 ? e["j"][4] : json());
        resume_pid_ = e["j"][0];
        next_pid_ = (std::max)(next_pid_, resume_pid_ + 1);
      } else if (e.contains("f")) {
        auto it = game_.heroes.find(e["f"][0].get<int>());
        if (it != game_.heroes.end()) game_.free_action(*it->second, e["f"][1]);
      } else if (e.contains("s")) {
        std::map<int, json> acts;
        for (auto& [k, v] : e["s"].items()) acts[std::stoi(k)] = v;
        game_.step(acts);
      }
    }
    game_.fx.clear();
    for (auto& [id, h] : game_.heroes) {
      h->log.clear();
      h->say(game_.depth == 0 ? std::string("You take up the dive again, in Charnfen.")
                              : "You take up the dive again, on floor " + std::to_string(game_.depth) + ".", 0xD2B48C);
    }
    if (game_.state != "play" || !resume_pid_) { resume_pid_ = 0; finished(); }
  } else if (!journal_.empty()) {
    std::ofstream f(journal_, std::ios::trunc);
    f << json{{"build", PYREHEART_BUILD}, {"seed", seed}, {"frost", game_.frost}}.dump() << "\n";
  }
  loop_ = std::thread([this] { loop(); });
  if (open) {
    accept_ = std::thread([this] { accept(); });
    beacon_ = std::thread([this] { beacon(); });
  }
}

Host::~Host() {
  running_ = false;
  if (server_ >= 0) { close_sock(sock_t(server_)); }
  if (udp_ >= 0) { close_sock(sock_t(udp_)); }
  if (loop_.joinable()) loop_.join();
  if (accept_.joinable()) accept_.detach();
  if (beacon_.joinable()) beacon_.detach();
}

std::shared_ptr<Link> Host::local() {
  struct Local : Link {
    Host* host;
    std::shared_ptr<Peer> peer;
    std::shared_ptr<Queue> in = std::make_shared<Queue>();
    void send(const json& m) override { std::lock_guard<std::mutex> l(host->mu_); host->inbox_.push_back({peer, m}); }
    std::optional<json> poll() override { return in->pop(); }
    bool closed() const override { return !host->running_; }
    ~Local() override {
      peer->dead = true;
      std::lock_guard<std::mutex> l(host->mu_);
      host->inbox_.push_back({peer, json{{"bye", true}}});
    }
  };
  auto link = std::make_shared<Local>();
  link->host = this;
  link->peer = std::make_shared<Peer>();
  auto q = link->in;
  link->peer->out = [q](const std::string& line) { q->push(json::parse(line)); };
  std::lock_guard<std::mutex> l(mu_);
  peers_.push_back(link->peer);
  return link;
}

void Host::accept() {
  sock_t ss = ::socket(AF_INET, SOCK_STREAM, 0);
  int yes = 1;
  setsockopt(ss, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(PORT);
  a.sin_addr.s_addr = INADDR_ANY;
  if (bad(ss) || ::bind(ss, (sockaddr*)&a, sizeof a) < 0 || ::listen(ss, 8) < 0) {
    std::lock_guard<std::mutex> l(mu_);
    error_ = "Could not open port " + std::to_string(PORT) + " for other players.";
    return;
  }
  server_ = intptr_t(ss);
  while (running_) {
    sock_t s = ::accept(ss, nullptr, nullptr);
    if (bad(s)) { if (!running_) return; continue; }
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
    auto peer = std::make_shared<Peer>();
    auto wmu = std::make_shared<std::mutex>();
    peer->out = [s, wmu, peer](const std::string& line) {
      if (peer->dead) return;
      std::lock_guard<std::mutex> l(*wmu);
      if (!send_all(s, line + "\n")) peer->dead = true;
    };
    {
      std::lock_guard<std::mutex> l(mu_);
      peers_.push_back(peer);
    }
    std::thread([this, s, peer] {
      read_lines(s, [&](const std::string& line) {
        if (line.size() > 20000) return false;
        json j = json::parse(line, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return false;
        std::lock_guard<std::mutex> l(mu_);
        inbox_.push_back({peer, j});
        return running_.load();
      });
      peer->dead = true;
      {
        std::lock_guard<std::mutex> l(mu_);
        inbox_.push_back({peer, json{{"bye", true}}});
      }
      close_sock(s);
    }).detach();
  }
}

// Answers "PYREHEART?" on UDP with "PYREHEART <host's first hero name>", so others on the same network find the game.
void Host::beacon() {
  sock_t s = ::socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(PORT);
  a.sin_addr.s_addr = INADDR_ANY;
  if (bad(s) || ::bind(s, (sockaddr*)&a, sizeof a) < 0) return;
  udp_ = intptr_t(s);
  char buf[64];
  while (running_) {
    sockaddr_in from{};
    socklen_t fl = sizeof from;
    int n = ::recvfrom(s, buf, sizeof buf - 1, 0, (sockaddr*)&from, &fl);
    if (n <= 0) { if (!running_) return; continue; }
    buf[n] = 0;
    if (std::strncmp(buf, "PYREHEART?", 10) != 0) continue;
    std::string name;
    {
      std::lock_guard<std::mutex> l(mu_);
      name = game_.heroes.empty() ? "a dive" : game_.heroes.begin()->second->name + "'s dive";
    }
    std::string reply = "PYREHEART " + name;
    ::sendto(s, reply.data(), int(reply.size()), 0, (sockaddr*)&from, fl);
  }
}

std::string Host::address() const {
  std::string out;
#ifndef _WIN32
  ifaddrs* ifs = nullptr;
  if (getifaddrs(&ifs) == 0) {
    for (ifaddrs* i = ifs; i; i = i->ifa_next) {
      if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
      char b[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, b, sizeof b);
      if (std::string(b).rfind("127.", 0) == 0) continue;
      out += (out.empty() ? "" : ", ") + std::string(b);
    }
    freeifaddrs(ifs);
  }
#else
  char name[256];
  if (gethostname(name, sizeof name) == 0) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    if (getaddrinfo(name, nullptr, &hints, &res) == 0) {
      for (addrinfo* r = res; r; r = r->ai_next) {
        char b[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in*)r->ai_addr)->sin_addr, b, sizeof b);
        out += (out.empty() ? "" : ", ") + std::string(b);
      }
      freeaddrinfo(res);
    }
  }
#endif
  return out;
}

void Host::write(const json& line) {
  if (journal_.empty()) return;
  std::ofstream f(journal_, std::ios::app);
  f << line.dump() << "\n";
}

// the dive is over: nothing to go on with
void Host::finished() {
  if (journal_.empty()) return;
  std::remove(journal_.c_str());
  journal_.clear();
}

void Host::loop() {
  while (running_) {
    bool changed = false;
    {
      std::lock_guard<std::mutex> l(mu_);
      while (!inbox_.empty()) {
        auto [p, m] = inbox_.front();
        inbox_.pop_front();
        handle(p, m);
      }
      if (!pending_.empty() && game_.state == "play") {
        bool all = true;
        for (auto* h : game_.active()) if (!pending_.count(h->pid)) all = false;
        if (all || now_ms() - first_pending_ > AUTO_WAIT_MS) {
          json s = json::object();
          for (auto& [pid, a] : pending_) s[std::to_string(pid)] = a;
          write({{"s", s}});
          game_.step(pending_);
          pending_.clear();
          if (game_.state != "play") finished();
          changed = true;
        }
      }
      if (changed) for (auto& p : peers_) push(*p);
      peers_.erase(std::remove_if(peers_.begin(), peers_.end(), [](auto& p) { return p->dead.load(); }), peers_.end());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(changed ? 1 : 10));
  }
}

void Host::handle(const std::shared_ptr<Peer>& peer, const json& m) {
  if (m.contains("bye")) {
    if (peer->pid) {
      game_.leave(peer->pid);
      pending_.erase(peer->pid);
      for (auto& p : peers_) if (!p->dead) push(*p);
    }
    return;
  }
  if (m.contains("hello") && m["hello"].is_object() && !peer->pid) {
    auto& hello = m["hello"];
    int here = 0;
    for (auto& [id, h] : game_.heroes) if (!h->away) ++here;
    if (here >= 4) { peer->out(json{{"error", "The party is full (4 heroes)."}}.dump()); return; }
    if (hello.value("ver", 0) != NET_VERSION) { peer->out(json{{"error", "This game is a different version of Pyreheart."}}.dump()); return; }
    if (resume_pid_) {  // the hero in the saved dive
      peer->pid = resume_pid_;
      resume_pid_ = 0;
      peer->out(json{{"welcome", peer->pid}}.dump());
      for (auto& p : peers_) push(*p);
      return;
    }
    if (hello.value("frost", 0) < game_.frost)  // a friend joins only at a Frost their hero has reached
      { peer->out(json{{"error", "This dive is at Frost " + std::to_string(game_.frost) + ". Your hero has not reached it yet."}}.dump()); return; }
    std::string name = hello.value("name", "Nameless").substr(0, 16);
    if (name.empty()) name = "Nameless";
    peer->pid = next_pid_++;
    json talents = hello.contains("talents") && hello["talents"].is_object() ? hello["talents"] : json::object();
    json record = hello.contains("hero") && hello["hero"].is_object() ? hello["hero"] : json();  // a hero kept from a dive won
    if (std::string why = game_.can_join(record); !why.empty()) { peer->pid = 0; peer->out(json{{"error", why}}.dump()); return; }
    Hero& h = game_.join(peer->pid, name, hello.value("cls", "warrior"), talents, record);
    json kept(h.talents);  // only what the tree allows
    write({{"j", {peer->pid, name, hello.value("cls", "warrior"), kept, record}}});
    peer->out(json{{"welcome", peer->pid}}.dump());
    for (auto& p : peers_) push(*p);
    return;
  }
  if (!m.contains("act") || !m["act"].is_object()) return;
  auto it = game_.heroes.find(peer->pid);
  if (it == game_.heroes.end()) return;
  Hero& h = *it->second;
  const json& a = m["act"];
  if (game_.free_action(h, a)) { write({{"f", {h.pid, a}}}); push(*peer); return; }
  if (!h.present()) return;
  if (pending_.empty()) first_pending_ = now_ms();
  pending_[h.pid] = a;
  if (game_.active().size() > 1) {
    json ready = json::array();
    for (auto& [pid, x] : pending_) ready.push_back(pid);
    for (auto& p : peers_) p->out(json{{"ready", ready}}.dump());
  }
}

void Host::push(Peer& p) {
  if (!p.pid || p.dead) return;
  auto it = game_.heroes.find(p.pid);
  if (it == game_.heroes.end()) return;
  Floor& f = *game_.floor;
  bool with_map = p.floor != f.id || p.version != f.version;
  p.floor = f.id; p.version = f.version;
  p.out(json{{"snap", view::snapshot(game_, *it->second, with_map)}}.dump());
}

// ------------------------------------------------------------------ joining someone else's game
namespace {
class SocketLink : public Link {
 public:
  explicit SocketLink(sock_t s) : s_(s) {
    reader_ = std::thread([this] {
      read_lines(s_, [this](const std::string& line) {
        json j = json::parse(line, nullptr, false);
        if (!j.is_discarded()) in_.push(std::move(j));
        return true;
      });
      closed_ = true;
    });
  }
  ~SocketLink() override {
    closed_ = true;
#ifdef _WIN32
    shutdown(s_, SD_BOTH);
#else
    shutdown(s_, SHUT_RDWR);
#endif
    close_sock(s_);
    if (reader_.joinable()) reader_.join();
  }
  void send(const json& m) override {
    std::lock_guard<std::mutex> l(w_);
    if (!send_all(s_, m.dump() + "\n")) closed_ = true;
  }
  std::optional<json> poll() override { return in_.pop(); }
  bool closed() const override { return closed_; }

 private:
  sock_t s_;
  std::mutex w_;
  Queue in_;
  std::atomic<bool> closed_{false};
  std::thread reader_;
};
}  // namespace

std::shared_ptr<Link> connect_to(const std::string& host, int port, std::string& err) {
  addrinfo hints{}, *res = nullptr;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
    err = "Cannot find " + host + ".";
    return nullptr;
  }
  sock_t s = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (bad(s) || ::connect(s, res->ai_addr, int(res->ai_addrlen)) < 0) {
    freeaddrinfo(res);
    if (!bad(s)) close_sock(s);
    err = "No game answers at " + host + ".";
    return nullptr;
  }
  freeaddrinfo(res);
  int yes = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof yes);
  return std::make_shared<SocketLink>(s);
}

std::vector<std::string> discover(int ms) {
  std::vector<std::string> out;
  sock_t s = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (bad(s)) return out;
  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof yes);
#ifdef _WIN32
  DWORD tv = 200;
#else
  timeval tv{0, 200000};
#endif
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(PORT);
  a.sin_addr.s_addr = INADDR_BROADCAST;
  const char* ask = "PYREHEART?";
  ::sendto(s, ask, int(std::strlen(ask)), 0, (sockaddr*)&a, sizeof a);
  int64_t end = now_ms() + ms;
  char buf[128];
  while (now_ms() < end) {
    sockaddr_in from{};
    socklen_t fl = sizeof from;
    int n = ::recvfrom(s, buf, sizeof buf - 1, 0, (sockaddr*)&from, &fl);
    if (n <= 0) continue;
    buf[n] = 0;
    if (std::strncmp(buf, "PYREHEART ", 10) != 0) continue;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, ip, sizeof ip);
    std::string entry = std::string(buf + 10) + "|" + ip;
    if (std::find(out.begin(), out.end(), entry) == out.end()) out.push_back(entry);
  }
  close_sock(s);
  return out;
}
