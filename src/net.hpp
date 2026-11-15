// The host keeps the one true game. Every player, the host's own too, talks to it through a Link: in-process for the
// host, a TCP socket for the others. Messages are lines of JSON.
//   player -> host: {"hello": {"name", "cls", "ver"}}, {"act": {...}}
//   host -> player: {"welcome": pid}, {"snap": {...}}, {"ready": [pids]}, {"error": "..."}
// The world steps when every hero has chosen; whoever has not chosen 4 seconds after the first did, waits.
// On a local network hosts also answer a UDP call on the same port, so phones and watches can find them.
#pragma once
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "game.hpp"

constexpr int PORT = 7777;
constexpr int NET_VERSION = 1;
constexpr int AUTO_WAIT_MS = 4000;

class Link {
 public:
  virtual ~Link() = default;
  virtual void send(const json& m) = 0;
  virtual std::optional<json> poll() = 0;
  virtual bool closed() const = 0;
};

class Host {
 public:
  Host(uint64_t seed, bool open);
  ~Host();
  std::shared_ptr<Link> local();
  std::string error() { std::lock_guard<std::mutex> l(mu_); return error_; }
  std::string address() const;  // this machine's addresses on the local network, for the others to type in

 private:
  struct Peer;
  Game game_;
  std::mutex mu_;
  std::vector<std::shared_ptr<Peer>> peers_;
  std::deque<std::pair<std::shared_ptr<Peer>, json>> inbox_;
  std::map<int, json> pending_;
  int64_t first_pending_ = 0;
  int next_pid_ = 1;
  std::atomic<bool> running_{true};
  std::string error_;
  std::thread loop_, accept_, beacon_;
  intptr_t server_ = -1, udp_ = -1;

  void loop();
  void accept();
  void beacon();
  void handle(const std::shared_ptr<Peer>& p, const json& m);
  void push(Peer& p);
};

std::shared_ptr<Link> connect_to(const std::string& host, int port, std::string& err);
/** Hosts answering on the local network within the given time: "name|address". */
std::vector<std::string> discover(int ms);
