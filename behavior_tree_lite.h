/// The recursive descent parser logic "back-inspired" by nom in Rust.
/// Most functions share the signature:
///
///   IResult parser(std::string_view);
///
/// where IResult is a container of the result type,
/// has either a valid value or an error type.
/// This is represented by `std::variant<Res<T>, std::string>`.
/// The first variant is the successful value and the second is
/// the error message string. In Rust's term, it is `Result<Res<T>, String>`.
/// The `Res<T>` type, in turn, represents a pair of the succeeding
/// string_view and a yielded value.
/// It is `std::pair<std::string_view, T>`, which is just a tuple
/// `(&str, T)` in Rust.

#ifndef BEHAVIOR_TREE_LITE_H
#define BEHAVIOR_TREE_LITE_H

#include <string_view>
#include <variant>
#include <optional>
#include <memory>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <exception>
#include <string>
#include <iostream>

namespace behavior_tree_lite {

enum class PortType {
    Input,
    Output,
    InOut,
};

/// The first variant is a variable reference. The second is a literal.
using BlackboardValue = std::variant<std::pair<std::string, PortType>, std::string>;

struct PortMap {
    PortType ty;
    /// Whether blackboard_variable is a literal instead of a variable name
    bool blackboard_literal;
    std::string node_port;
    BlackboardValue blackboard_value;
};

using PortMaps = std::vector<PortMap>;

struct VarDef;

struct TreeDef {
    std::string name;
    PortMaps port_maps;
    std::vector<TreeDef> children;
    std::vector<VarDef> vars;
};

struct PortDef {
    PortType direction;
    std::string name;
};

struct TreeRootDef {
    std::string name;
    TreeDef root;
    std::vector<PortDef> ports;
};

struct VarDef {
    std::string_view name;
    std::optional<std::string_view> init;
};

struct VarAssign {
    std::string_view name;
    std::string_view init;
};

using TreeElem = std::variant<TreeDef, VarDef, VarAssign>;


thread_local size_t indent_level = 1;

/// A dummy type to introduce indentation to ostream.
/// The nesting level is counted in thread local indent_level, which is not
/// exception safe.
struct indent_t{} indent;

std::ostream &operator<<(std::ostream& os, indent_t) {
    for (size_t i = 0; i < indent_level; i++) {
        os << "  ";
    }
    return os;
}

std::ostream &operator<<(std::ostream& os, const VarDef& var) {
    os << indent << "VarDef {\n";
    os << indent << "  .name = " << var.name << "\n";
    if (var.init) {
        os << indent << "  .init = " << *var.init << "\n";
    }
    os << indent << "}\n";
    return os;
}

std::ostream &operator<<(std::ostream& os, const TreeDef& node) {
    os << indent << "Node {\n";
    indent_level++;
    os << indent << ".name = \"" << node.name << "\",\n";
    os << indent << ".port_maps = [\n";
    indent_level++;
    for (auto& port_map : node.port_maps) {
        os << indent << port_map.node_port;
        switch (port_map.ty) {
            case PortType::Input:
                os << " <- ";
                break;
            case PortType::Output:
                os << " -> ";
                break;
            case PortType::InOut:
                os << " <-> ";
                break;
        }
        if (auto literal = std::get_if<1>(&port_map.blackboard_value)) {
            os << '"' << *literal << "\"\n";
        }
        else if (auto port = std::get_if<0>(&port_map.blackboard_value)) {
            os << port->first << "\n";
        }
    }
    indent_level--;
    os << indent << "],\n";

    os << indent << ".children = [\n";
    indent_level++;
    for (auto& child : node.children) {
        os << child;
    }
    indent_level--;
    os << indent << "]\n";
    os << indent << ".vars = [\n";
    indent_level++;
    for (auto& var : node.vars) {
        os << var;
    }
    indent_level--;
    os << indent << "]\n";
    indent_level--;
    os << indent << "}\n";
    return os;
}

template<typename T = std::string_view>
using Res = std::pair<std::string_view, T>;

template<typename T = std::string_view>
using IResult = std::variant<Res<T>, std::string>;

/// The infallible space skipper
Res<std::string_view> space(std::string_view i) {
    auto r = i;
    while (!r.empty() && isspace(r[0])) {
        r = r.substr(1);
    }
    return std::make_pair(r, i.substr(0, r.data() - i.data()));
}

/// The infallible empty lines skipper.
/// It is not the same as space, because a newline can be significant in some context.
Res<std::string_view> empty_lines(std::string_view i) {
    auto r = space(i).first;
    while (!r.empty() && (isspace(r[0]) || r[0] == '\r' || r[0] == '\n')) {
        r = r.substr(1);
    }
    return std::make_pair(r, i.substr(0, r.data() - i.data()));
}

IResult<std::string_view> identifier(std::string_view i) {
    i = space(i).first;

    if (i.empty() || (!isalpha(i[0]) && i[0] != '_')) {
        return std::string("Expected an identifier");
    }

    auto r = i;
    while (!r.empty() && (isalnum(r[0]) || r[0] == '_')) {
        r = r.substr(1);
    }
    IResult<std::string_view> ret = std::make_pair(r, i.substr(0, r.data() - i.data()));
    return ret;
}

void print_res(const Res<std::string_view>& res) {
    std::cout << "(" << res.first << ", " << res.second << ")";
}

template<char C>
IResult<std::string_view> match_char(std::string_view i) {
    i = space(i).first;

    if (i.empty() || i[0] != C) {
        char str[] = {C, '\0'};
        return std::string("Expected token '") + str + "'";
    }

    return std::make_pair(i.substr(1), i.substr(0, 1));
}

template<char C>
IResult<std::string_view> unmatch_char(std::string_view i) {
    i = space(i).first;

    if (!i.empty() && i[0] == C) {
        char str[] = {C, '\0'};
        return std::string("Expected token '") + str + "'";
    }

    return std::make_pair(i.substr(1), i.substr(0, 1));
}


IResult<TreeElem> parse_tree_child(std::string_view i);
IResult<TreeElem> parse_condition_node(std::string_view i);
inline IResult<TreeElem> var_decl(std::string_view i);

inline IResult<std::vector<TreeElem>> tree_children(std::string_view i) {
    auto r = i;
    std::vector<TreeElem> ret;
    while (!r.empty()) {
        auto res = parse_tree_child(r);
        auto pair = std::get_if<0>(&res);
        if (!pair) {
            break;
        }
        ret.push_back(std::move(pair->second));
        r = pair->first;
    }
    return std::make_pair(r, ret);
}

inline IResult<std::vector<TreeElem>> tree_children_block(std::string_view i) {
    i = space(i).first;
    auto res2 = match_char<'{'>(i);
    if (auto e = std::get_if<1>(&res2)) {
        return *e;
    }
    auto res3 = tree_children(std::get<0>(res2).first);
    if (auto e = std::get_if<1>(&res3)) {
        return *e;
    }
    auto res4 = match_char<'}'>(std::get<0>(res3).first);
    if (auto e = std::get_if<1>(&res4)) {
        return *e;
    }
    auto r4 = std::get<0>(res4);

    auto children = std::get<0>(res3).second;

    return std::make_pair(r4.first, children);
}

IResult<std::string_view> string_literal(std::string_view i) {
    i = space(i).first;
    auto res2 = match_char<'"'>(i);
    if (auto e = std::get_if<1>(&res2)) {
        return *e;
    }

    auto r = i.substr(1);
    while (!r.empty()) {
        auto res2 = unmatch_char<'"'>(r);
        if (auto e = std::get_if<1>(&res2)) {
            break;
        }
        r = std::get<0>(res2).first;
    }

    return std::make_pair(r.substr(1), i.substr(1, r.data() - i.data() - 1));
}

IResult<PortMap> port_map(std::string_view i) {
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) {
        return *e;
    }
    auto pair = std::get<0>(res);
    auto r = space(pair.first).first;

    std::string_view next;
    PortType ty;
    if (r.substr(0, 2) == "<-") {
        next = r.substr(2);
        ty = PortType::Input;
    }
    else if (r.substr(0, 2) == "->") {
        next = r.substr(2);
        ty = PortType::Output;
    }
    else if (r.substr(0, 3) == "<->") {
        next = r.substr(3);
        ty = PortType::InOut;
    }
    else {
        return std::string("Expected \"<-\", \"->\" or \"<->\"");
    }

    bool blackboard_literal = false;
    BlackboardValue blackboard_value;
    auto res2 = string_literal(next);
    if (auto pair2 = std::get_if<0>(&res2)) {
        next = pair2->first;
        blackboard_literal = true;
        blackboard_value = std::string(pair2->second);
    }
    else {
        auto res2 = identifier(next);
        if (auto e = std::get_if<1>(&res2)) {
            return *e;
        }
        auto pair3 = std::get<0>(res2);
        next = pair3.first;
        blackboard_value = std::make_pair(std::string(pair3.second), ty);
    }

    PortMap port_map {
        .ty = ty,
        .blackboard_literal = blackboard_literal,
        .node_port = std::string(pair.second),
        .blackboard_value = blackboard_value,
    };

    return std::make_pair(next, port_map);
}

IResult<PortMaps> port_maps(std::string_view i) {
    auto r = i;
    PortMaps ret;
    while (!r.empty()) {
        auto res = port_map(r);
        auto pair = std::get_if<0>(&res);
        if (!pair) {
            break;
        }
        ret.push_back(pair->second);
        r = pair->first;
        auto res4 = match_char<','>(r);
        if (auto e = std::get_if<1>(&res4)) {
            break;
        }
        r = std::get<0>(res4).first;
    }
    return std::make_pair(r, ret);
}

IResult<PortMaps> port_maps_parens(std::string_view i) {
    i = space(i).first;
    auto res2 = match_char<'('>(i);
    if (auto e = std::get_if<1>(&res2)) {
        return *e;
    }
    auto res3 = port_maps(std::get<0>(res2).first);
    if (auto e = std::get_if<1>(&res3)) {
        return *e;
    }
    auto res4 = match_char<')'>(std::get<0>(res3).first);
    if (auto e = std::get_if<1>(&res4)) {
        return *e;
    }
    auto r4 = std::get<0>(res4);

    auto port_maps = std::get<0>(res3).second;

    return std::make_pair(r4.first, port_maps);
}

inline TreeDef tree_def_with_ports(std::string_view name, std::string init) {
    PortMaps port_maps;
    port_maps.push_back(PortMap {
        .ty = PortType::Input,
        .blackboard_literal = true,
        .node_port = "value",
        .blackboard_value = std::string(init),
    });
    port_maps.push_back(PortMap {
        .ty = PortType::Output,
        .blackboard_literal = false,
        .node_port = "output",
        .blackboard_value = std::make_pair(std::string(name), PortType::Output),
    });
    return TreeDef {
        .name = "SetBool",
        .port_maps = port_maps,
        .children = std::vector<TreeDef>{},
    };
}

inline TreeDef tree_def_from_elems(std::string name, PortMaps port_maps, std::vector<TreeElem> elems) {
    std::vector<TreeDef> children;
    std::vector<VarDef> vars;
    for (auto& tree_elem : elems) {
        if (auto tree_def = std::get_if<TreeDef>(&tree_elem)) {
            children.push_back(std::move(*tree_def));
        }
        else if (auto var_def = std::get_if<VarDef>(&tree_elem)) {
            if (var_def->init) {
                children.push_back(tree_def_with_ports(var_def->name, std::string(*var_def->init)));
            }
            vars.push_back(std::move(*var_def));
        }
    }
    return TreeDef {
        .name = name,
        .port_maps = std::move(port_maps),
        .children = std::move(children),
        .vars = std::move(vars),
    };
}

inline IResult<TreeDef> parse_tree_node(std::string_view i) {
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) {
        return std::string("Expected node name: " + *e);
    }
    auto r = std::get<0>(res);
    auto name = std::string(r.second);

    auto next = r.first;
    PortMaps port_maps;
    auto ports_res = port_maps_parens(next);
    if (auto ports = std::get_if<0>(&ports_res)) {
        next = ports->first;
        port_maps = ports->second;
    }

    auto res2 = tree_children_block(next);
    TreeDef ret; 
    if (auto r3 = std::get_if<0>(&res2)) {
        next = r3->first;
        ret = tree_def_from_elems(name, std::move(port_maps), r3->second);
    }
    else {
        ret = tree_def_from_elems(name, std::move(port_maps), {});
    }
    return std::make_pair(next, ret);
}

inline IResult<TreeElem> parse_condition_node(std::string_view i) {
    auto r2 = space(i).first;
    auto res2 = match_char<'('>(r2);
    if (auto e = std::get_if<1>(&res2)) return *e;
    auto res3 = parse_tree_node(std::get<0>(res2).first);
    if (auto e = std::get_if<1>(&res3)) return *e;
    auto [r3, condition] = std::get<0>(res3);
    auto res4 = match_char<')'>(r3);
    if (auto e = std::get_if<1>(&res4)) return *e;
    auto [next, _] = std::get<0>(res4);
    auto res5 = tree_children_block(next);
    std::vector<TreeDef> children{condition};
    if (auto r5 = std::get_if<0>(&res5)) {
        next = r5->first;
        auto true_br = tree_def_from_elems("Sequence", PortMaps{}, std::move(r5->second));
        children.push_back(std::move(true_br));
    }
    next = space(next).first;
    if (next.substr(0, 4) == "else") {
        auto res6 = tree_children_block(next.substr(4));
        if (auto e = std::get_if<1>(&res6)) return *e;
        auto r6 = std::get<0>(res6);
        auto false_br = tree_def_from_elems("Sequence", PortMaps{}, std::move(r6.second));
        children.push_back(std::move(false_br));
        next = r6.first;
    }
    return std::make_pair(next, TreeDef {
        .name = "if",
        .port_maps = PortMaps{},
        .children = std::move(children),
        .vars = std::vector<VarDef>{},
    });
}

inline IResult<TreeElem> parse_tree_child(std::string_view i) {
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) return *e;
    auto r = std::get<0>(res);

    if (r.second == "if") {
        auto res = parse_condition_node(r.first);
        if (auto e = std::get_if<1>(&res)) return *e;
        auto r2 = std::get<0>(res);
        return std::make_pair(r2.first, TreeElem{r2.second});
    }

    if (r.second == "var") {
        return var_decl(r.first);
    }

    auto res2 = parse_tree_node(i);
    if (auto e = std::get_if<1>(&res2)) return *e;
    auto r2 = std::get<0>(res2);
    return std::make_pair(r2.first, TreeElem{r2.second});
}

inline IResult<TreeElem> var_decl(std::string_view i) {
    i = space(i).first;
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) return *e;
    auto r2 = std::get<0>(res);
    auto next = r2.first;
    auto name = r2.second;
    next = space(next).first;

    std::optional<std::string_view> init;
    if (next.substr(0, 1) == "=") {
        auto init_res2 = identifier(next.substr(1));
        if (auto e = std::get_if<1>(&init_res2)) return *e;
        auto init_r = std::get<0>(init_res2);
        if (init_r.second != "true" && init_r.second != "false") {
            return std::string("true or false expected as the initializer");
        }
        init = init_r.second;
        next = init_r.first;
    }

    return std::make_pair(next, VarDef {
        .name = name,
        .init = init,
    });
}

struct Tree {
    std::string name;
    TreeDef node;
    std::vector<PortDef> ports;
};

inline std::ostream &operator<<(std::ostream& os, const Tree& tree) {
    os << indent << "Tree {\n";
    indent_level++;
    os << indent << ".name = \"" << tree.name << "\",\n";
    os << indent << ".node = \n" << tree.node;
    indent_level--;
    os << indent << "}\n";
    return os;
}

inline std::ostream &operator<<(std::ostream& os, const std::vector<Tree>& trees) {
    os << indent << "[\n";
    indent_level++;
    for (auto& tree : trees) {
        os << tree;
    }
    indent_level--;
    os << indent << "]\n";
    return os;
}

inline IResult<PortDef> port_def(std::string_view i) {
    i = space(i).first;
    auto first = identifier(i);
    if (auto e = std::get_if<1>(&first)) {
        return *e;
    }
    auto first_ok = std::get<0>(first);

    PortType direction;
    if (first_ok.second == "in") {
        direction = PortType::Input;
    }
    else if (first_ok.second == "out") {
        direction = PortType::Output;
    }
    else if (first_ok.second == "inout") {
        direction = PortType::InOut;
    }

    auto res = identifier(first_ok.first);
    if (auto e = std::get_if<1>(&res)) {
        return *e;
    }
    auto res_ok = std::get<0>(res);

    return std::make_pair(res_ok.first, PortDef {
        .direction = direction,
        .name = std::string(res_ok.second),
    });
}

inline IResult<std::vector<PortDef>> subtree_ports_def(std::string_view i) {
    i = space(i).first;
    if (i.empty() || i[0] != '(') {
        return std::string("Expected opening paren '('");
    }
    auto r = i.substr(1);

    std::vector<PortDef> result;

    while (!r.empty()) {
        auto res = port_def(r);
        auto pair = std::get_if<0>(&res);
        if (!pair) {
            break;
        }
        result.push_back(pair->second);
        r = pair->first;
        auto res4 = match_char<','>(r);
        if (auto e = std::get_if<1>(&res4)) {
            break;
        }
        r = std::get<0>(res4).first;
    }

    r = space(r).first;
    if (r.empty() || r[0] != ')') {
        return std::string("Expected closing paren ')'");
    }
    r = r.substr(1);

    return std::make_pair(r, result);
}

IResult<Tree> parse_tree(std::string_view i) {
    i = empty_lines(i).first;
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) {
        return std::string("Did not recognize the first identifier: " + *e);
    }

    auto r = std::get<0>(res);
    if (r.second != "tree") {
        return std::string("The first identifier must be \"tree\"");
    }

    auto res2 = identifier(r.first);
    if (auto e = std::get_if<1>(&res2)) {
        return std::string("Missing tree name: " + *e);
    }
    auto name_ok = std::get<0>(res2);
    auto r2 = name_ok.first;

    std::vector<PortDef> port_defs;
    auto res3 = subtree_ports_def(r2);
    if (auto res3_ok = std::get_if<0>(&res3)) {
        port_defs = res3_ok->second;
        r2 = res3_ok->first;
    }

    auto r3 = space(r2).first;
    if (r3.empty() || r3[0] != '=') {
        return std::string("Tree name should be followed by a equal (=)");
    }
    // Skip the equal
    auto r4 = r3.substr(1);

    auto res5 = parse_tree_child(r4);
    if (auto e = std::get_if<1>(&res5)) {
        return std::string("TreeDef parse error: " + *e);
    }
    auto r5 = std::get<0>(res5);
    // Eat extra newlines after the last node
    auto r6 = empty_lines(r5.first);

    if (auto t_def = std::get_if<TreeDef>(&r5.second)) {
        return std::make_pair(r6.first, Tree{
            .name = std::string(name_ok.second),
            .node = std::move(*t_def),
            .ports = port_defs,
        });
    }
    else {
        return std::string("Tree root cannot be a variable definition");
    }
}

using TreeSource = std::vector<Tree>;

IResult<TreeSource> source_text(std::string_view i) {
    std::vector<Tree> ret;

    while (!i.empty()) {
        auto res = parse_tree(i);
        if (auto e = std::get_if<1>(&res)) {
            return *e;
        }
        auto pair = std::get<0>(res);
        ret.push_back(pair.second);
        i = pair.first;
    }

    return std::make_pair(i, ret);
}

enum class BehaviorResult {
    Success,
    Fail,
    /// The node should keep running in the next tick
    Running,
};

using BBMap = std::unordered_map<std::string, BlackboardValue>;

struct Context;
struct BehaviorNodeContainer;

class BehaviorNode {
public:
    virtual BehaviorResult tick(Context& context) = 0;
};

struct Registry {
    std::unordered_map<std::string, std::function<std::unique_ptr<BehaviorNode> ()>> node_types;
    std::unordered_map<std::string, std::string> key_names;
};

using Blackboard = std::unordered_map<std::string, std::string>;

class undefined_port_error : public std::exception {
    const char* what() const noexcept override {
        return "Attempt to assign to an undefined port";
    }
};

class undefined_variable_error : public std::exception {
    const char* what() const noexcept override {
        return "Could not find the named variable";
    }
};

class undefined_node_error : public std::exception {
    std::string name;
public:
    undefined_node_error(std::string name) : name(std::move(name)) {}
    const char* what() const noexcept override {
        thread_local std::string ret;
        ret = std::string("Could not find the node type name: ") + name;
        return ret.c_str();
    }
};

class write_input_port_error : public std::exception {
    const char* what() const noexcept override {
        return "Attempt to assign to a input port";
    }
};

class write_to_literal_error : public std::exception {
    const char* what() const noexcept override {
        return "Attempt to assign to a literal";
    }
};

class invalid_count_error : public std::exception {
    const char* what() const noexcept override {
        return "Invalid count string";
    }
};

struct Context {
    Blackboard blackboard;
    BBMap *blackboard_map;
    std::vector<BehaviorNodeContainer>* child_nodes;
//    bool strict;

    std::optional<std::string> get(const std::string& port_name) const {
        auto var_it = blackboard_map->find(port_name);
        if (var_it != blackboard_map->end()) {
            if (auto x = std::get_if<0>(&var_it->second)) {
                if (x->second == PortType::Output) return std::nullopt;
                auto y = blackboard.find(x->first);
                if (y == blackboard.end()) return std::nullopt;
                return y->second;
            }
            if (auto x = std::get_if<1>(&var_it->second)) {
                return *x;
            }
        }
        return std::nullopt;
    }

    void set(const std::string& port_name, const std::string& value) {
        auto var_it = blackboard_map->find(port_name);
        if (var_it == blackboard_map->end()) {
            throw undefined_port_error{};
        }
        if (auto x = std::get_if<0>(&var_it->second)) {
            if (x->second == PortType::Input) {
                throw write_input_port_error{};
            }
            blackboard[x->first] = value;
            return;
        }
        throw write_to_literal_error{};
    }

    std::optional<BehaviorResult> tick_child(int idx);
};

struct BehaviorNodeContainer {
    /// Name of the type of the node
    std::string name;
protected:
    std::unique_ptr<BehaviorNode> node;
    BBMap blackboard_map;
    std::vector<BehaviorNodeContainer> child_nodes;

public:
    BehaviorNodeContainer(
        std::string name,
        std::unique_ptr<BehaviorNode> node,
        BBMap bbmap,
        std::vector<BehaviorNodeContainer> child_nodes
    ) : name(name),
        node(std::move(node)),
        blackboard_map(std::move(bbmap)),
        child_nodes(std::move(child_nodes)) { }

    BehaviorResult tick(Context& context) {
        if (this->node) {
            auto prev_child_node = context.child_nodes;
            context.child_nodes = &this->child_nodes;
            auto prev_blackboard_map = context.blackboard_map;
            context.blackboard_map = &this->blackboard_map;
            BehaviorResult res;
            try {
                res = this->node->tick(context);
            }
            catch (std::exception &e) {
                context.child_nodes = prev_child_node;
                context.blackboard_map = prev_blackboard_map;
                throw;
            }
            context.child_nodes = prev_child_node;
            context.blackboard_map = prev_blackboard_map;
            return res;
        }
        return BehaviorResult::Success;
    }

    const std::vector<BehaviorNodeContainer>& get_child_nodes() const {
        return child_nodes;
    }
};

inline std::optional<BehaviorResult> Context::tick_child(int idx) {
    auto children = this->child_nodes;
    if (children->size() <= idx) return std::nullopt;
    auto& child = (*children)[idx];
    return child.tick(*this);
}

class SequenceNode : public BehaviorNode {
    int current_child = 0;
    BehaviorResult tick(Context& context) override {
        BehaviorResult result = BehaviorResult::Success;
        while (current_child < context.child_nodes->size()) {
            result = (*context.child_nodes)[current_child].tick(context);
            bool break_out = false;
            switch (result) {
                case BehaviorResult::Success: current_child++; break;
                case BehaviorResult::Fail: current_child++; break_out = true; break;
                case BehaviorResult::Running: break_out = true; break;
            }
            if (break_out) break;
        }
        if (current_child == context.child_nodes->size()) {
            current_child = 0;
        }

        return result;
    }
};

class ReactiveSequenceNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        int current_child = 0;
        BehaviorResult result = BehaviorResult::Success;
        while (current_child < context.child_nodes->size()) {
            result = (*context.child_nodes)[current_child].tick(context);
            bool break_out = false;
            switch (result) {
                case BehaviorResult::Success: current_child++; break;
                case BehaviorResult::Fail: current_child++; break_out = true; break;
                case BehaviorResult::Running: break_out = true; break;
            }
            if (break_out) break;
        }

        return result;
    }
};

class FallbackNode : public BehaviorNode {
    int current_child = 0;
    BehaviorResult tick(Context& context) override {
        BehaviorResult result = BehaviorResult::Fail;
        while (current_child < context.child_nodes->size()) {
            result = (*context.child_nodes)[current_child].tick(context);
            bool break_out = false;
            switch (result) {
                case BehaviorResult::Success: current_child++; break_out = true; break;
                case BehaviorResult::Fail: current_child++; break;
                case BehaviorResult::Running: break_out = true; break;
            }
            if (break_out) {
                current_child++;
                break;
            }
        }
        if (current_child == context.child_nodes->size()) {
            current_child = 0;
        }

        return result;
    }
};

class ReactiveFallbackNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        BehaviorResult result = BehaviorResult::Fail;
        int current_child = 0;
        while (current_child < context.child_nodes->size()) {
            result = (*context.child_nodes)[current_child].tick(context);
            bool break_out = false;
            switch (result) {
                case BehaviorResult::Success: current_child++; break_out = true; break;
                case BehaviorResult::Fail: current_child++; break;
                case BehaviorResult::Running: break_out = true; break;
            }
            if (break_out) {
                current_child++;
                break;
            }
        }

        return result;
    }
};

class ForceSuccessNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto first = context.child_nodes->begin();
        if (first != context.child_nodes->end()) {
            auto result = first->tick(context);
            switch (result) {
                case BehaviorResult::Running: return BehaviorResult::Running;
            }
        }

        return BehaviorResult::Success;
    }
};

class ForceFailureNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto first = context.child_nodes->begin();
        if (first != context.child_nodes->end()) {
            auto result = first->tick(context);
            switch (result) {
                case BehaviorResult::Running: return BehaviorResult::Running;
            }
        }

        return BehaviorResult::Fail;
    }
};

class InverterNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        BehaviorResult result = BehaviorResult::Fail;
        const auto child = context.child_nodes->begin();
        if (child != context.child_nodes->end()) {
            result = child->tick(context);
            switch (result) {
                case BehaviorResult::Success: result = BehaviorResult::Fail; break;
                case BehaviorResult::Fail: result = BehaviorResult::Success; break;
            }
        }

        return result;
    }
};

class RepeatNode : public BehaviorNode {
    int n = 0;
    BehaviorResult tick(Context& ctx) override {
        BehaviorResult result = BehaviorResult::Fail;
        auto n = ctx.get("n");
        if (!n) throw invalid_count_error();
        if (this->n == 0) this->n = std::atoi(n->c_str());
        if (!this->n) {
            throw invalid_count_error();
        }
        this->n--;
        if (this->n == 0) {
            return BehaviorResult::Success;
        }
        auto res = ctx.tick_child(0);
        if (!res) return BehaviorResult::Fail;
        if (res == BehaviorResult::Success) {
            return BehaviorResult::Running;
        }
        else if (res == BehaviorResult::Running) {
            return BehaviorResult::Running;
        }
        this->n = 0;
        return *res;
    }
};

class RetryNode : public BehaviorNode {
    int n = 0;
    BehaviorResult tick(Context& ctx) override {
        BehaviorResult result = BehaviorResult::Fail;
        auto n = ctx.get("n");
        if (!n) throw invalid_count_error();
        if (this->n == 0) this->n = std::atoi(n->c_str());
        if (!this->n) {
            throw invalid_count_error();
        }
        this->n--;
        if (this->n == 0) {
            return BehaviorResult::Success;
        }
        auto res = ctx.tick_child(0);
        if (!res) return BehaviorResult::Fail;
        if (res == BehaviorResult::Fail) {
            return BehaviorResult::Running;
        }
        else if (res == BehaviorResult::Running) {
            return BehaviorResult::Running;
        }
        this->n = 0;
        return *res;
    }
};

class TrueNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        return BehaviorResult::Success;
    }
};

class FalseNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        return BehaviorResult::Fail;
    }
};

class SetBoolNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto var_it = context.get("value");
        if (var_it) {
            context.set("output", *var_it);
        }

        return BehaviorResult::Success;
    }
};

class IfNode : public BehaviorNode {
    std::optional<BehaviorResult> condition_result;
    BehaviorResult tick(Context& ctx) override {
        auto res = ctx.tick_child(0);
        if (res == BehaviorResult::Fail) {
            auto res2 = ctx.tick_child(2);
            if (res2) return *res2;
            return BehaviorResult::Fail;
        }
        auto res2 = ctx.tick_child(1);
        if (res2) return *res2;
        return BehaviorResult::Fail;
    }
};

struct PortSpec {
    PortType ty;
    std::string key;

    static PortSpec new_in(const std::string& key) {
        return PortSpec {
            .ty = PortType::Input,
            .key = key,
        };
    }

    static PortSpec new_out(const std::string& key) {
        return PortSpec {
            .ty = PortType::Output,
            .key = key,
        };
    }

    static PortSpec new_inout(const std::string& key) {
        return PortSpec {
            .ty = PortType::InOut,
            .key = key,
        };
    }
};


/// SubtreeNode is a container for a subtree, introducing a local namescope of blackboard variables.
struct SubtreeNode : public BehaviorNode {
    /// Blackboard variables needs to be a part of the node payload
    Blackboard blackboard;
    std::vector<PortSpec> params;

    SubtreeNode(Blackboard blackboard, std::vector<PortSpec> params) :
        blackboard(std::move(blackboard)), params(std::move(params)) {
    }

    BehaviorResult tick(Context& ctx) override {
        BehaviorResult res = BehaviorResult::Success;
        for (auto& param : params) {
            if (param.ty != PortType::Input && param.ty != PortType::InOut) {
                continue;
            }
            if (auto value = ctx.get(param.key)) {
                blackboard[param.key] = *value;
            }
        }

        std::swap(blackboard, ctx.blackboard);
        auto sub_res = ctx.tick_child(0);
        if (sub_res) res = *sub_res;
        std::swap(blackboard, ctx.blackboard);

        // It is debatable if we should assign the output value back to the parent blackboard
        // when the result was Fail or Running. We chose to assign them, which seems less counterintuitive.
        for (auto& param : params) {
            if (param.ty != PortType::Output || param.ty != PortType::InOut) {
                continue;
            }
            auto value = blackboard.find(param.key);
            if (value != blackboard.end()) {
                ctx.set(param.key, value->second);
            }
        }

        return res;
    }
};

Registry defaultRegistry() {
    Registry registry;

    registry.node_types.emplace(std::string("Sequence"),
        std::function([](){ return std::make_unique<SequenceNode>(); }));
    registry.node_types.emplace(std::string("ReactiveSequence"),
        std::function([](){ return std::make_unique<ReactiveSequenceNode>(); }));
    registry.node_types.emplace(std::string("Fallback"),
        std::function([](){ return std::make_unique<FallbackNode>(); }));
    registry.node_types.emplace(std::string("ReactiveFallbackStar"),
        std::function([](){ return std::make_unique<ReactiveFallbackNode>(); }));
    registry.node_types.emplace(std::string("ForceSuccess"),
        std::function([](){ return std::make_unique<ForceSuccessNode>(); }));
    registry.node_types.emplace(std::string("ForceFailure"),
        std::function([](){ return std::make_unique<ForceFailureNode>(); }));
    registry.node_types.emplace(std::string("Inverter"),
        std::function([](){ return std::make_unique<InverterNode>(); }));
    registry.node_types.emplace(std::string("Repeat"),
        std::function([](){ return std::make_unique<RepeatNode>(); }));
    registry.node_types.emplace(std::string("Retry"),
        std::function([](){ return std::make_unique<RetryNode>(); }));
    registry.node_types.emplace(std::string("true"),
        std::function([](){ return std::make_unique<TrueNode>(); }));
    registry.node_types.emplace(std::string("false"),
        std::function([](){ return std::make_unique<FalseNode>(); }));
    registry.node_types.emplace(std::string("SetBool"),
        std::function([](){ return std::make_unique<SetBoolNode>(); }));
    registry.node_types.emplace(std::string("if"),
        std::function([](){ return std::make_unique<IfNode>(); }));

    return registry;
}

BehaviorNodeContainer load_recurse(
    const TreeDef& parent,
    const TreeSource& tree_source,
    const Registry& registry
) {
    std::vector<BehaviorNodeContainer> child_nodes;

    std::unique_ptr<BehaviorNode> node;
    auto tree_it = std::find_if(tree_source.begin(), tree_source.end(), [&parent](auto& tree) {
        return tree.name == parent.name;
    });
    if (tree_it != tree_source.end()) {
        std::vector<PortSpec> port_specs;
        std::transform(tree_it->ports.begin(), tree_it->ports.end(), std::back_inserter(port_specs),
            [](auto& port) {
                return PortSpec {
                    .ty = port.direction,
                    .key = port.name,
                };
            });
        node = std::make_unique<SubtreeNode>(Blackboard{}, port_specs);
        child_nodes.push_back(load_recurse(tree_it->node, tree_source, registry));
    }
    else {
        std::transform(parent.children.begin(), parent.children.end(), std::back_inserter(child_nodes),
        [&tree_source, &registry](auto& child){
            return load_recurse(child, tree_source, registry);
        });

        auto node_it = registry.node_types.find(parent.name);
        if (node_it == registry.node_types.end()) {
            throw undefined_node_error{parent.name};
        }
        node = node_it->second();
    }

    BBMap bbmap;
    for (auto& port_map : parent.port_maps) {
        bbmap.emplace(port_map.node_port, port_map.blackboard_value);
    }

    return BehaviorNodeContainer(
        parent.name,
        std::move(node),
        std::move(bbmap),
        std::move(child_nodes)
    );
}

/// Instantiate a behavior tree from a AST of a tree.
///
/// `check_ports` enables static checking of port availability before actually ticking.
/// It is useful to catch errors in a behavior tree source file, but you need to
/// implement [`crate::BehaviorNode::provided_ports`] to use it.
std::optional<BehaviorNodeContainer> load(
    TreeSource& tree_source,
    const Registry& registry
) {
    auto main_it = std::find_if(tree_source.begin(), tree_source.end(), [](auto& tree) {
        return tree.name == "main";
    });

    if (main_it == tree_source.end()) {
        return std::nullopt;
    }

    auto tree_con = load_recurse(main_it->node, tree_source, registry);

    return tree_con;
}

BehaviorResult tick_node(BehaviorNodeContainer& node, Blackboard &bb) {
    Context context { .blackboard = bb };
    return node.tick(context);
}

}

#endif // BEHAVIOR_TREE_LITE_H
