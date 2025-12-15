#include <string_view>
#include <variant>

struct Node {
	std::string name;
};

std::ostream &operator<<(std::ostream& os, const Node& node) {
	os << "Node { .name = \"" << node.name << "\" }";
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

IResult<Node> parse_node(std::string_view i) {
	auto res = identifier(i);
	if (auto e = std::get_if<1>(&res)) {
		return std::string("Expected node name: " + *e);
	}
	auto r = std::get<0>(res);

	return std::make_pair(r.first, Node { .name = std::string(r.second) });
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

	auto res5 = parse_node(r4);
	if (auto e = std::get_if<1>(&res5)) {
		return std::string("Node parse error: " + *e);
	}
	auto r5 = std::get<0>(res5);

	return std::make_pair(r5.first, Tree{
		.name = std::string(r2.second),
		.node = r5.second
	});
}

