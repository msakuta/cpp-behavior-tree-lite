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

#include <string_view>
#include <variant>
#include <optional>
#include <memory>
#include <algorithm>

enum class PortType {
    Input,
    Output,
    Inout,
};

struct PortMap {
    PortType ty;
    /// Whether blackboard_variable is a literal instead of a variable name
    bool blackboard_literal;
    std::string node_port;
    std::string blackboard_variable;
};

using PortMaps = std::vector<PortMap>;

struct TreeDef {
    std::string name;
    PortMaps port_maps;
    std::vector<TreeDef> children;
};

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
            case PortType::Inout:
                os << " <-> ";
                break;
        }
        if (port_map.blackboard_literal) {
            os << '"' << port_map.blackboard_variable << "\"\n";
        }
        else {
            os << port_map.blackboard_variable << "\n";
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


IResult<TreeDef> parse_tree_node(std::string_view i);

IResult<std::vector<TreeDef>> tree_children(std::string_view i) {
    auto r = i;
    std::vector<TreeDef> ret;
    while (!r.empty()) {
        auto res = parse_tree_node(r);
        auto pair = std::get_if<0>(&res);
        if (!pair) {
            break;
        }
        ret.push_back(std::move(pair->second));
        r = pair->first.substr(1);
    }
    return std::make_pair(r, ret);
}

IResult<std::vector<TreeDef>> tree_children_block(std::string_view i) {
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
        ty = PortType::Inout;
    }
    else {
        return std::string("Expected \"<-\", \"->\" or \"<->\"");
    }

    bool blackboard_literal = false;
    std::string blackboard_variable;
    auto res2 = string_literal(next);
    if (auto pair2 = std::get_if<0>(&res2)) {
        next = pair2->first;
        blackboard_literal = true;
        blackboard_variable = std::string(pair2->second);
    }
    else {
        auto res2 = identifier(next);
        if (auto e = std::get_if<1>(&res2)) {
            return *e;
        }
        auto pair3 = std::get<0>(res2);
        next = pair3.first;
        blackboard_variable = std::string(pair3.second);
    }

    PortMap port_map {
        .ty = ty,
        .blackboard_literal = blackboard_literal,
        .node_port = std::string(pair.second),
        .blackboard_variable = blackboard_variable,
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

IResult<TreeDef> parse_tree_node(std::string_view i) {
    auto res = identifier(i);
    if (auto e = std::get_if<1>(&res)) {
        return std::string("Expected node name: " + *e);
    }
    auto r = std::get<0>(res);

    auto next = r.first;
    PortMaps port_maps;
    auto ports_res = port_maps_parens(next);
    if (auto ports = std::get_if<0>(&ports_res)) {
        next = ports->first;
        port_maps = ports->second;
    }

    auto res2 = tree_children_block(next);
    std::vector<TreeDef> children;
    if (auto r3 = std::get_if<0>(&res2)) {
        next = r3->first;
        children = r3->second;
    }
    return std::make_pair(next, TreeDef {
        .name = std::string(r.second),
        .port_maps = port_maps,
        .children = children,
    });
}

struct Tree {
    std::string name;
    TreeDef node;
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

IResult<Tree> parse_tree(std::string_view i) {
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

    auto r2 = std::get<0>(res2);
    auto r3 = space(r2.first);
    if (r3.first.empty() || r3.first[0] != '=') {
        return std::string("Tree name should be followed by a equal (=)");
    }
    // Skip the equal
    auto r4 = r3.first.substr(1);

    auto res5 = parse_tree_node(r4);
    if (auto e = std::get_if<1>(&res5)) {
        return std::string("TreeDef parse error: " + *e);
    }
    auto r5 = std::get<0>(res5);

    return std::make_pair(r5.first, Tree{
        .name = std::string(r2.second),
        .node = r5.second
    });
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

struct Context;

class BehaviorNode {
public:
    virtual BehaviorResult tick(Context& context) = 0;
};

struct BehaviorNodeContainer {
    /// Name of the type of the node
    std::string name;
    std::unique_ptr<BehaviorNode> node;
    std::vector<BehaviorNodeContainer> child_nodes;
};

struct Context {
//    Blackboard blackboard;
//    BBMap blackboard_map;
    std::vector<BehaviorNodeContainer>* child_nodes;
//    bool strict;
};

class SequenceNode : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        std::cout << "SequenceNode ticked!!\n";

        std::vector<BehaviorNodeContainer>* prev_child_nodes = context.child_nodes;
        std::cout << "prev_child_nodes: " << (prev_child_nodes->size()) << "\n";
        for (auto& node : *prev_child_nodes) {
            context.child_nodes = &node.child_nodes;
            if (node.node) {
                node.node->tick(context);
            }
        }
        context.child_nodes = prev_child_nodes;

        return BehaviorResult::Success;
    }
};

BehaviorNodeContainer load_recurse(
    const TreeDef& parent,
    const TreeSource& tree_source
) {
    std::vector<BehaviorNodeContainer> child_nodes;
    std::transform(parent.children.begin(), parent.children.end(), std::back_inserter(child_nodes),
        [&tree_source](auto& child){
            return load_recurse(child, tree_source);
        });

    std::unique_ptr<BehaviorNode> node;
    if (parent.name == "Sequence") {
        node = std::make_unique<SequenceNode>();
    }

    return BehaviorNodeContainer {
        .name = parent.name,
        .node = std::move(node),
        .child_nodes = std::move(child_nodes),
    };
}

/// Instantiate a behavior tree from a AST of a tree.
///
/// `check_ports` enables static checking of port availability before actually ticking.
/// It is useful to catch errors in a behavior tree source file, but you need to
/// implement [`crate::BehaviorNode::provided_ports`] to use it.
std::optional<BehaviorNodeContainer> load(
    TreeSource& tree_source
) {
    auto main_it = std::find_if(tree_source.begin(), tree_source.end(), [](auto& tree) {
        return tree.name == "main";
    });

    if (main_it == tree_source.end()) {
        return std::nullopt;
    }

    auto tree_con = load_recurse(main_it->node, tree_source);

    return tree_con;
}

void tick_node(BehaviorNodeContainer& node) {
    Context context {
        .child_nodes = &node.child_nodes,
    };

    node.node->tick(context);
}

