// Two heroes over a real socket: the host's own and one from "another computer" on 127.0.0.1.
#include <chrono>
#include <cstdio>
#include <thread>

#include "net.hpp"

using namespace std::chrono_literals;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

struct Side {
  std::shared_ptr<Link> link;
  int pid = 0, turn = -1, snaps = 0;
  json last;
  void drain() {
    while (auto m = link->poll()) {
      if (m->contains("welcome")) pid = (*m)["welcome"];
      if (m->contains("snap")) { last = (*m)["snap"]; ++snaps; turn = last.value("turn", -1); }
      if (m->contains("error")) std::printf("error: %s\n", (*m)["error"].dump().c_str());
    }
  }
};

static void settle(Side& a, Side& b, int ms = 300) {
  for (int i = 0; i < ms / 10; ++i) { a.drain(); b.drain(); std::this_thread::sleep_for(10ms); }
}

int main() {
  Host host(42, true);
  CHECK(host.error().empty());
  Side a{host.local()};
  a.link->send({{"hello", {{"name", "Aldo"}, {"cls", "warrior"}, {"ver", NET_VERSION}}}});
  std::string err;
  auto remote = connect_to("127.0.0.1", PORT, err);
  CHECK(remote != nullptr);
  if (!remote) { std::printf("connect: %s\n", err.c_str()); return 1; }
  Side b{remote};
  b.link->send({{"hello", {{"name", "Bea"}, {"cls", "adept"}, {"ver", NET_VERSION}}}});
  settle(a, b);
  CHECK(a.pid == 1);
  CHECK(b.pid == 2);
  CHECK(b.last.is_object() && b.last["heroes"].size() == 2);
  CHECK(b.last.contains("map"));
  int t0 = a.turn;

  // one acts: the world waits for the other
  a.link->send({{"act", {{"t", "wait"}}}});
  settle(a, b);
  CHECK(a.turn == t0);
  // both acted: it steps
  b.link->send({{"act", {{"t", "wait"}}}});
  settle(a, b);
  CHECK(a.turn == t0 + 1);
  CHECK(b.turn == t0 + 1);

  // one acts and the other dawdles: after the wait it steps anyway
  a.link->send({{"act", {{"t", "wait"}}}});
  for (int i = 0; i < (AUTO_WAIT_MS + 800) / 10; ++i) { a.drain(); b.drain(); std::this_thread::sleep_for(10ms); }
  CHECK(a.turn == t0 + 2);

  // the second leaves; the first plays on alone
  b.link.reset();
  remote.reset();
  settle(a, a, 500);
  a.link->send({{"act", {{"t", "wait"}}}});
  settle(a, a, 300);
  CHECK(a.turn == t0 + 3);

  // a wrong version is turned away
  auto c = connect_to("127.0.0.1", PORT, err);
  Side cs{c};
  c->send({{"hello", {{"name", "Old"}, {"cls", "ranger"}, {"ver", NET_VERSION + 1}}}});
  bool refused = false;
  for (int i = 0; i < 50 && !refused; ++i) {
    while (auto m = c->poll()) if (m->contains("error")) refused = true;
    std::this_thread::sleep_for(10ms);
  }
  CHECK(refused);
  std::printf(fails ? "net: %d failed\n" : "net: all good\n", fails);
  return fails ? 1 : 0;
}
