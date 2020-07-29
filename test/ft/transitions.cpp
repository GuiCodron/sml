//
// Copyright (c) 2016-2020 Kris Jusiak (kris at jusiak dot net)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <boost/sml.hpp>

namespace sml = boost::sml;

struct e1 {};
struct e2 {};
struct e3 {};
struct e4 {};
struct e5 {};
struct e6 {};

const auto idle = sml::state<class idle>;
const auto s1 = sml::state<class s1>;
const auto s2 = sml::state<class s2>;
const auto s3 = sml::state<class s3>;
const auto s4 = sml::state<class s4>;

test transition = [] {
  using namespace sml;
  struct c {
    // clang-format off
    auto operator()() noexcept {
      return make_transition_table(
        *idle + event<e1> = s1
      );
    }
    // clang-format on
  };

  sml::sm<c> sm;
  expect(sm.is(idle));
  sm.process_event(e1{});
  expect(sm.is(s1));
};

test internal_transition = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1 + event<e2> / [] {}
        , s1 + event<e3> = s2
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  expect(sm.is(idle));
  sm.process_event(e1{});
  expect(sm.is(s1));
  sm.process_event(e2{});
  expect(sm.is(s1));
  sm.process_event(e3{});
  expect(sm.is(s2));
};

test anonymous_transition = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
       *idle / [this] { a_called = true; } = s1
      );
      // clang-format on
    }

    bool a_called = false;
  };

  sml::sm<c> sm{};
  expect(sm.is(s1));
  expect(static_cast<const c&>(sm).a_called);
};

test subsequent_anonymous_transitions = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
       *idle / [this] { a_calls.push_back(1); } = s1
       ,s1 / [this] { a_calls.push_back(2); } = s2
       ,s2 / [this] { a_calls.push_back(3); } = s3
      );
      // clang-format on
    }

    std::vector<int> a_calls{};
  };

  sml::sm<c> sm{};
  expect(sm.is(s3));
  expect(static_cast<const c&>(sm).a_calls == std::vector<int>{{1, 2, 3}});
};

test subsequent_anonymous_transitions_composite = [] {
  using V = std::string;
  V calls{};

  using namespace sml;

  struct sub_sub_sm {
    auto operator()() noexcept {
      // clang-format off
      return make_transition_table(
       *idle / [] (V& v) { v+="ss1|"; } = s1
       ,s1 / [] (V& v) { v+="ss2|"; } = s2
       ,s2 / [] (V& v) { v+="ss3|"; } = X
      );
      // clang-format on
    }
  };
  struct sub_sm {
    auto operator()() noexcept {
      // clang-format off
      return make_transition_table(
       *idle / [] (V& v) { v+="s1|"; } = s1
       ,s1 / [] (V& v) { v+="s2|"; } = s2
       ,s2 / [] (V& v) { v+="s3|"; } = state<sub_sub_sm>
       ,state<sub_sub_sm> + sml::on_entry<_> / [] (V& v) { v+="ssen|"; }
       ,state<sub_sub_sm> + sml::on_exit<_> / [] (V& v) { v+="ssex|"; }
       ,state<sub_sub_sm> / [] (V& v) { v+="s4|"; } = X
      );
      // clang-format on
    }
  };

  struct composite_sm {
    auto operator()() noexcept {
      // clang-format off
      return make_transition_table(
       *idle / [] (V& v) { v+="11|"; } = s1
       ,s1 / [] (V& v) { v+="12|"; } = state<sub_sm>
       ,state<sub_sm> / [] (V& v) { v+="13|"; } = s2
       ,s2 / [] (V& v) { v+="14|"; } = s3
      );
      // clang-format on
    }
  };

  sml::sm<composite_sm> sm{calls};
  expect(sm.is<decltype(state<sub_sm>)>(X));
  expect(sm.is(s3));
  std::string expected("11|12|s1|s2|s3|ssen|ss1|ss2|ss3|ssex|s4|13|14|");
  expect(calls == expected);
};

test self_transition = [] {
  enum class calls { s1_entry, s1_exit, s1_action };

  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
        *idle = s1 // anonymous-transition
       , s1 + event<e1> / [] (std::vector<calls>& c) { c.push_back(calls::s1_action); } = s1 // self-transition
       , s1 + sml::on_entry<_> / [] (std::vector<calls>& c) { c.push_back(calls::s1_entry); } // internal-transition
       , s1 + sml::on_exit<_>  / [] (std::vector<calls>& c) { c.push_back(calls::s1_exit); } // internal-transition
      );
      // clang-format on
    }
  };

  std::vector<calls> c_;
  sml::sm<c> sm{c_};
  expect(std::vector<calls>{calls::s1_entry} == c_);

  c_.clear();
  sm.process_event(e1{});
  expect(std::vector<calls>{calls::s1_exit, calls::s1_action, calls::s1_entry} == c_);
};

test no_transition = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      return make_transition_table(*idle + event<e1> = s1);
    }
  };

  sml::sm<c> sm;
  expect(sm.is(idle));
  sm.process_event(e2{});
  expect(sm.is(idle));
  sm.process_event(e3{});
  expect(sm.is(idle));
};

test transition_with_action_with_event = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto action = [this](const e1&) { called = true; };
      return make_transition_table(*idle + event<e1> / action = s1);
    }

    bool called = false;
  };

  sml::sm<c> sm;
  const c& c_ = sm;
  expect(sm.is(idle));
  sm.process_event(e1{});
  expect(c_.called);
  expect(sm.is(s1));
};

test transition_with_action_with_parameter = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto action = [this](int i) {
        called = true;
        expect(i == 42);
      };
      return make_transition_table(*idle + event<e1> / action = s1);
    }

    bool called = false;
  };

  sml::sm<c> sm{42};
  const c& c_ = sm;
  sm.process_event(e1{});
  expect(c_.called);
  expect(sm.is(s1));
};

test transition_with_action_and_guad_with_parameter = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;

      auto guard = [this](double d) {
        g_called = true;
        expect(d == 87.0);
        return true;
      };

      auto action = [this](int i) {
        a_called = true;
        expect(i == 42);
      };

      // clang-format off
      return make_transition_table(
        *idle + event<e1> [guard] / action = s1
      );
      // clang-format on
    }

    bool a_called = false;
    bool g_called = false;
  };

  sml::sm<c> sm{87.0, 42};
  const c& c_ = sm;
  sm.process_event(e1{});
  expect(c_.g_called);
  expect(c_.a_called);
  expect(sm.is(s1));
};

test transition_with_action_and_guad_with_parameters_and_event = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;

      auto guard = [this](int i, const e1&, double d) {
        g_called = true;
        expect(i == 42);
        expect(d == 87.0);
        return true;
      };

      auto action = [this](int i, float& f) {
        a_called = true;
        expect(i == 42);
        expect(f == 12.f);
      };

      // clang-format off
      return make_transition_table(
        *idle + event<e1> [guard] / action = s1
      );
      // clang-format on
    }

    bool a_called = false;
    bool g_called = false;
  };

  auto f = 12.f;
  sml::sm<c> sm{42, 87.0, f};
  const c& c_ = sm;
  sm.process_event(e1{});
  expect(c_.g_called);
  expect(c_.a_called);
  expect(sm.is(s1));
};

test transitions = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto yes = [] { return true; };
      auto no = [] { return false; };

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1 + event<e2> = s2
        , s2 + event<e3> [no] = s3
        , s2 + event<e3> [yes] = s4
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  sm.process_event(e1{});
  sm.process_event(e2{});
  sm.process_event(e3{});
  expect(sm.is(s4));
};

test transitions_dsl = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto yes = [] { return true; };
      auto no = [] { return false; };

      // clang-format off
      return make_transition_table(
          s1 <= *idle + event<e1>
        , s2 <= s1 + event<e2>
        , s3 <= s2 + event<e3> [no]
        , s4 <= s2 + event<e3> [yes]
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  sm.process_event(e1{});
  sm.process_event(e2{});
  sm.process_event(e3{});
  expect(sm.is(s4));
};

test transitions_dsl_mix = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto yes = [] { return true; };
      auto no = [] { return false; };

      // clang-format off
      return make_transition_table(
          s1 <= *idle + event<e1>
        , s1 + event<e2> = s2
        , s3 <= s2 + event<e3> [no]
        , s2 + event<e3> [yes] = s4
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  sm.process_event(e1{});
  sm.process_event(e2{});
  sm.process_event(e3{});
  expect(sm.is(s4));
};

test transition_loop = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1 + event<e2> = s2
        , s2 + event<e3> = idle
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  expect(sm.is(idle));
  sm.process_event(e1{});
  expect(sm.is(s1));
  sm.process_event(e2{});
  expect(sm.is(s2));
  sm.process_event(e3{});
  expect(sm.is(idle));
  sm.process_event(e1{});
  expect(sm.is(s1));
};

test no_transitions = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto yes = [] { return true; };
      auto no = [] { return false; };

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1 + event<e2> [no] = s2
        , s1 + event<e2> [yes] = s2
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  sm.process_event(e1{});
  sm.process_event(e3{});
  sm.process_event(e2{});
  sm.process_event(e1{});
  expect(sm.is(s2));
};

test transitions_states = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto yes = [] { return true; };
      auto no = [] { return false; };

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1 + event<e2> = s2
        , s2 + event<e3> [no] = s3
        , s2 + event<e3> [yes] = s4
      );
      // clang-format on
    }
  };

  sml::sm<c> sm;
  sm.process_event(e1{});
  sm.process_event(e2{});
  sm.process_event(e3{});
  using namespace sml;
  expect(sm.is(s4));
};

test transition_overload = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , s1   + event<e2> = s2
        , s1   + event<e3> = s3
      );
      // clang-format on
    }
  };

  {
    sml::sm<c> sm;
    sm.process_event(e1{});
    sm.process_event(e2{});
    expect(sm.is(s2));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e1{});
    sm.process_event(e3{});
    expect(sm.is(s3));
  }
};

test initial_transition_overload = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , idle + event<e2> = s2
      );
      // clang-format on
    }
  };

  {
    sml::sm<c> sm;
    sm.process_event(e1{});
    expect(sm.is(s1));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e2{});
    expect(sm.is(s2));
  }
};

test initial_entry = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
         *idle + sml::on_entry<_> / [this] { ++entry_calls; }
      );
      // clang-format on
    }

    int entry_calls = 0;
  };

  sml::sm<c> sm{};
  const c& c_ = sm;
  expect(1 == c_.entry_calls);
};
template <typename T>
struct DBG;

test initial_nontrivial_entry = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
         *idle + sml::on_entry<e2> / [this] { entry_calls+=2; }
         ,idle + sml::on_entry<_> / [this] { entry_calls-=1; }
         ,idle + event<e2> = s1
         ,s1 + on_entry<_> / [this] { entry_calls-=1; }
         ,s1 + event<e3> = s2
         ,s2 + on_entry<e3> / [this] { entry_calls+=3; }
         ,s2 + on_entry<e2> / [this] { entry_calls+=2; }
         ,s2 + on_entry<e1> / [this] { entry_calls+=1; }
         ,s2 + on_entry<_>  / [this] { --entry_calls; }
         ,s2 + event<e3> = s3
         ,s3 + on_entry<e2> / [this] { entry_calls+=2; }
         ,s3 + on_entry<e1> / [this] { entry_calls+=1; }
         ,s3 + on_entry<_> / [this] { --entry_calls; }
      );
      // clang-format on
    }

    int entry_calls = 0;
  };

  struct d {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
        *idle + event<e2> = state<c>
      );
      // clang-format on
    }
  };
  {
    using namespace sml;
    using _ = back::_;
    // using c_sm_impl_t = back::sm_impl<back::sm_policy<c>>;
    // using d_sm_impl_t = back::sm_impl<back::sm_policy<c>>;
    // DBG<back::mappings_t<c_sm_impl_t::transitions_t>>{};
    // DBG<back::get_event_mapping_t<back::get_mapped_t<back::on_entry<_, e2>>, c_sm_impl_t::mappings>>{};
    // DBG<c_sm_impl_t::transitions_t>{};
    // using transitions_t = sm_impl_t::transitions_t;

    // DBG<back::on_entry<_, e1>>{};
    // DBG<back::get_mapped_t<back::on_entry<_, _>>>{};
    // DBG<back::get_generic_t<back::on_entry<_, _>>>{};
    // DBG<aux::is_base_of<back::get_mapped_t<back::on_entry<_, _>>, events_ids_t>>{};
    // DBG<aux::is_base_of<back::get_generic_t<back::on_entry<_, _>>, events_ids_t>>{};

    // DBG<decltype(on_entry<e1>)>{};
    // DBG<back::get_mapped_t<decltype(on_entry<e1>)>>{};
    // DBG<back::get_generic_t<decltype(on_entry<e1>)>>{};

    // DBG<decltype(on_entry<_>)>{};
    // DBG<back::get_mapped_t<decltype(on_entry<_>)>>{};
    // DBG<back::get_generic_t<decltype(on_entry<_>)>>{};

    std::cout << "\n\n\n\n\n\nMy test ont_entry\n";

    sml::sm<c> sm{};
    const c& c_ = sm;
    std::cout << c_.entry_calls << "\n";
    expect(-1 == c_.entry_calls);
    sm.process_event(e2{});
    std::cout << c_.entry_calls << "\n";
    expect(sm.is(s1));
    expect(-2 == c_.entry_calls);
    sm.process_event(e3{});
    std::cout << c_.entry_calls << "\n";
    expect(1 == c_.entry_calls);
    sm.process_event(e3{});
    std::cout << c_.entry_calls << "\n";
    expect(0 == c_.entry_calls);
  }
  {
    sml::sm<d> sm{};
    const c& c_ = sm;
    sm.process_event(e2{});
    std::cout << c_.entry_calls << "\n";
    expect(2 == c_.entry_calls);
    sm.process_event(e2{});
    std::cout << c_.entry_calls << "\n";
    expect(1 == c_.entry_calls);
    sm.process_event(e3{});
    expect(4 == c_.entry_calls);
    sm.process_event(e3{});
    expect(3 == c_.entry_calls);
  }
};

test initial_nontrivial_exit = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
         *idle + sml::on_exit<_> / [this] { calls+="_|"; }
         ,idle + sml::on_exit<e2> / [this] { calls+="e2|"; }
         ,idle + event<e1> = s1
         ,idle + event<e2> = s1
         ,s1 + sml::on_exit<e2> / [this] { calls+="e2|"; }
         ,s1 + sml::on_exit<e1> / [this] { calls+="e1|"; }
         ,s1 + sml::on_exit<_> / [this] { calls+="_|"; }
         ,s1 + event<e3> = s2
         ,s1 + event<e1> = s2
         ,s2 + sml::on_exit<e3>  / [this] { calls+="e3|"; }
         ,s2 + sml::on_exit<e2> / [this] { calls+="e2|"; }
         ,s2 + sml::on_exit<e1> / [this] { calls+="e1|"; }
         ,s2 + sml::on_exit<_> / [this] { calls+="_|"; }
         ,s2 + event<e3> = s3
      );
      // clang-format on
    }

    std::string calls;
  };

  struct d {
    auto operator()() noexcept {
      using namespace sml;
      // clang-format off
      return make_transition_table(
        *state<c> + event<e2> = idle
        , idle + on_entry<_> / []{std::cout << "idle\n";}
      );
      // clang-format on
    }
  };
  std::cout << "\n\n\n\n\n\nMy test on_exit\n";
  {
    sml::sm<c> sm{};
    const c& c_ = sm;
    sm.process_event(e1{});
    expect(sm.is(s1));
    std::cout << c_.calls << "\n";
    expect("_|" == c_.calls);
    sm.process_event(e3{});
    expect(sm.is(s2));
    std::cout << c_.calls << "\n";
    expect("_|_|" == c_.calls);

    sm.process_event(e3{});
    std::cout << c_.calls << "\n";
    expect("_|_|e3|" == c_.calls);
    expect(sm.is(s3));
  }
  {
    sml::sm<d> sm{};
    const c& c_ = sm;

    sm.process_event(e1{});
    std::cout << c_.calls << "\n";
    expect("_|" == c_.calls);

    sm.process_event(e1{});
    std::cout << c_.calls << "\n";
    expect("_|e1|" == c_.calls);

    sm.process_event(e2{});
    std::cout << c_.calls << "\n";
    expect("_|e1|e2|" == c_.calls);
  }
};

#if !defined(_MSC_VER)
test general_transition_overload = [] {
  struct c {
    auto operator()() noexcept {
      using namespace sml;
      auto is_e3_or_e4 = [](auto event) {
        return std::is_same<e3, typename std::decay<decltype(event)>::type>::value ||
               std::is_same<e4, typename std::decay<decltype(event)>::type>::value;
      };
      auto is_e5_or_e6 = [](auto event) {
        return std::is_same<e5, typename std::decay<decltype(event)>::type>::value ||
               std::is_same<e6, typename std::decay<decltype(event)>::type>::value;
      };

      // clang-format off
      return make_transition_table(
         *idle + event<e1> = s1
        , idle + event<_> [is_e3_or_e4] = s3
        , idle + event<e2> = s2
        , idle + event<_> [is_e5_or_e6] = s4  // Only e5 will match this line, because
        , idle + event<e6> = s1               // this line is the better match for e6.
        // Non-reachable states, just to make some events not 'unexpected'!
        // Only event e4 is really 'unexpected'.
        , X + event<e3> / []{}
        , X + event<e5> / []{}
        , X + event<e6> / []{}
      );
      // clang-format on
    }
  };

  {
    sml::sm<c> sm;
    sm.process_event(e1{});
    expect(sm.is(s1));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e2{});
    expect(sm.is(s2));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e3{});
    expect(sm.is(s3));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e4{});
    expect(sm.is(idle));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e5{});
    expect(sm.is(s4));
  }

  {
    sml::sm<c> sm;
    sm.process_event(e6{});
    expect(sm.is(s1));
  }
};
#endif
