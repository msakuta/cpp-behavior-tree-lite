#include <iostream>
#include <string>
#include <vector>
#include "parser.h"

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
        std::function([](){ return static_cast<std::unique_ptr<BehaviorNode>>(
            std::make_unique<PrintNode>()); }));
    registry.node_types.emplace(std::string("GetValue"),
        std::function([](){ return static_cast<std::unique_ptr<BehaviorNode>>(
            std::make_unique<GetValueNode>()); }));
    auto tree = load(pair.second, registry);

    std::cout << "Tree instantiated: " << !!tree << "\n";
    if (tree) {
        std::cout << "  Name: " << tree->name << "\n";
        auto& child_nodes = tree->get_child_nodes();
        std::cout << "  Children: " << child_nodes.size() << "\n";
        for (auto& child : child_nodes) {
            std::cout << "    " << child.name << "\n";
        }

        Blackboard bb;
        bb["foo"] = "bar";

        tick_node(*tree, bb);
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
    GetValue (output -> bbValue)
    })";

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
    test_blackboard();
    return 0;
}

