#include <iostream>
#include <string>
#include <vector>
#include "behavior_tree_lite.h"

using namespace behavior_tree_lite;

void test_identifier() {
    std::string src = "Hello world lorem ipsum some_thing\n";

    std::vector<std::string_view> tokens;

    std::string_view cursor = src;

    while (!cursor.empty()) {
        auto res = identifier(cursor);

        if (auto e = std::get_if<1>(&res)) {
            std::cout << "Error: " << *e << "\n";
            break;
        }
        else {
            auto pair = std::get<Res<std::string_view>>(res);
            print_res(pair);
            tokens.push_back(pair.second);
            cursor = pair.first;
        }
    }

    std::cout << "Tokens: " << tokens.size() << "\n";
    for (auto& token : tokens) {
        std::cout << token << "\n";
    }
}

class PrintNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto var_it = context.get("input");
        if (var_it) {
            std::cout << "Print(\"" << *var_it << "\")\n";
        }
        else {
            std::cout << "Print could not find input port\n";
        }

        return BehaviorResult::Success;
    }
};

class GetValueNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        std::cout << "GetValue()\n";

        return BehaviorResult::Success;
    }
};

class CountDownNode : public BehaviorNode {
    int count = -1;
    BehaviorResult tick(Context& context) override {
        if (count < 0) {
            auto bb_count = context.get("count");
            if (bb_count) {
                count = std::atoi(bb_count->c_str());
            }
        }
        std::cout << "CountDown ticks " << count << "\n";
        if (0 < --count) {
            return BehaviorResult::Running;
        }

        return BehaviorResult::Success;
    }
};


void build_and_run(std::string_view src) {
    auto res = source_text(src);

    if (auto e = std::get_if<1>(&res)) {
        std::cout << "Error: " << *e << "\n";
        return;
    }

    auto pair = std::get<0>(res);
    std::cout << "Tree parsed: " << pair.second << ", remainder: \"" << pair.first << "\"\n";

    auto registry = defaultRegistry();
    registry.node_types.emplace(std::string("Print"),
        std::function([](){ return std::make_unique<PrintNode>(); }));
    registry.node_types.emplace(std::string("GetValue"),
        std::function([](){ return std::make_unique<GetValueNode>(); }));
    registry.node_types.emplace(std::string("CountDown"),
        std::function([](){ return std::make_unique<CountDownNode>(); }));
    auto tree = load(pair.second, registry);

    std::cout << "Tree instantiated: " << !!tree << "\n";
    if (tree) {
        Blackboard bb;
        bb["foo"] = "bar";

        try {
            BehaviorResult res;
            do {
                res = tick_node(*tree, bb);
            } while (res == BehaviorResult::Running);
        }
        catch(std::exception& e) {
            std::cout << "Error in tick_node: " << e.what() << "\n";
        }
    }
}


void test_tree() {
    std::string src = R"(tree main = Sequence {
    Print (input <- "hey")
    GetValue (output -> bbValue)
    })";

    build_and_run(src);
}

void test_fallback_tree() {
    std::string src = R"(tree main = Fallback {
    Print (input <- "hey")
    GetValue (output -> bbValue)
    })";

    build_and_run(src);
}

void test_blackboard() {
    std::string src = R"(tree main = Sequence {
    Print (input <- foo)
    SetValue (input <- "Hey", output -> foo)
    Print (input <- foo)
    })";

    build_and_run(src);
}

void test_blackboard_create_var() {
    std::string src = R"(tree main = Sequence {
    Print (input <- foo)
    SetValue (input <- "Hey", output -> bar)
    Print (input <- bar)
    })";

    build_and_run(src);
}

void test_blackboard_err() {
    std::string src = R"(tree main = Sequence {
    SetValue (input <- "Hey", non_existent_port_name -> bar)
    })";

    build_and_run(src);
}

void test_countdown() {
    std::string src = R"(tree main = Sequence {
    CountDown (count <- "3")
    Print(input <- "Boom!")
    })";

    build_and_run(src);
}

void test_subtree() {
    std::string src = R"(tree main = Sequence {
    SubTree(param <- "Hello")
}

tree SubTree(in param) = Sequence {
    Print(input <- param)
}
)";

    build_and_run(src);
}

void test_true() {
    std::string src = R"(tree main = Sequence {
    true
    Print(input <- "Hello")
}
)";

    build_and_run(src);
}

void test_inverter() {
    std::string src = R"(tree main = Sequence {
    Inverter {
        false
    }
    Print(input <- "Hello")
}
)";

    build_and_run(src);
}

void test_repeat() {
    std::string src = R"(tree main = Repeat(n <- "5") {
    Print(input <- "Hello")
}
)";

    build_and_run(src);
}

void test_repeat_fail() {
    std::string src = R"(tree main = Sequence {
    Repeat(n <- "5") {
        Sequence {
            Print(input <- "Hello")
            false
        }
    }
}
)";

    build_and_run(src);
}

void test_retry() {
    std::string src = R"(tree main = Retry(n <- "5") {
    Print(input <- "Hello")
}
)";

    build_and_run(src);
}

void test_retry_fail() {
    std::string src = R"(tree main = Sequence {
    Retry(n <- "5") {
        Sequence {
            Print(input <- "Hello")
            false
        }
    }
}
)";

    build_and_run(src);
}

void test_conditional_true() {
    std::string src = R"(tree main = if (true) {
    Print(input <- "Got true")
}
)";

    build_and_run(src);
}

void test_conditional_false() {
    std::string src = R"(tree main = if (false) {
    Print(input <- "Got true")
}
)";

    build_and_run(src);
}

void test_string_literal() {
    std::string src = R"(  "hey"   )";
    auto res = string_literal(src);

    if (auto e = std::get_if<1>(&res)) {
        std::cout << "Error: " << *e << "\n";
    }

    auto pair = std::get<0>(res);
    std::cout << "String literal: " << pair.second << ", remainder: \"" << pair.first << "\n";
}

int main() {
    //test_tree();
    //test_fallback_tree();
    //test_string_literal();
    //test_blackboard();
    //test_blackboard_create_var();
    //test_blackboard_err();
    //test_countdown();
    //test_subtree();
    //test_true();
    //test_inverter();
    //test_repeat();
    //test_repeat_fail();
    //test_retry();
    //test_retry_fail();
    test_conditional_false();
    return 0;
}

