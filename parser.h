/// The parser logic "back-inspired" by nom in Rust.
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

struct Node {
	std::string name;
	std::vector<Node> children;
};

std::ostream &operator<<(std::ostream& os, const Node& node) {
	os << "Node { .name = \"" << node.name << "\",\n    .children = [";
	for (auto& child : node.children) {
		os << child << "\n";
	}
	os << "]}";
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

IResult<Node> parse_tree_node(std::string_view i);

IResult<std::vector<Node>> tree_children(std::string_view i) {
	auto r = i;
	std::vector<Node> ret;
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

IResult<std::vector<Node>> tree_children_block(std::string_view i) {
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

IResult<Node> parse_tree_node(std::string_view i) {
	auto res = identifier(i);
	if (auto e = std::get_if<1>(&res)) {
		return std::string("Expected node name: " + *e);
	}
	auto r = std::get<0>(res);

	auto next = r.first;
	auto res2 = tree_children_block(r.first);
	std::vector<Node> children;
	if (auto r3 = std::get_if<0>(&res2)) {
		children = r3->second;
		next = r3->first;
	}
	return std::make_pair(next, Node {
		.name = std::string(r.second),
		.children = children,
	});
}

struct Tree {
	std::string name;
	Node node;
};

inline std::ostream &operator<<(std::ostream& os, const Tree& tree) {
	os << "Tree { .name = \"" << tree.name << "\", .node = " << tree.node << " }";
	return os;
}

IResult<Tree> source_text(std::string_view i) {
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
		return std::string("Node parse error: " + *e);
	}
	auto r5 = std::get<0>(res5);

	return std::make_pair(r5.first, Tree{
		.name = std::string(r2.second),
		.node = r5.second
	});
}

