#include <string_view>
#include <variant>

struct Node {
};

template<typename T = std::string_view>
using Res = std::pair<std::string_view, T>;

template<typename T = std::string_view>
using IResult = std::variant<Res<T>, std::string>;

IResult<std::string_view> identifier(std::string_view i) {
	while (!i.empty() && isspace(i[0])) {
		i = i.substr(1);
	}

	if (i.empty() || (!isalpha(i[0]) && i[0] != '_')) {
		return std::string("Undefined");
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

struct Tree {
	std::string name;
};

inline std::ostream &operator<<(std::ostream& os, const Tree& tree) {
	os << "Tree { .name = \"" << tree.name << "\" }";
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


	return std::make_pair(r2.first, Tree{ .name = std::string(r2.second) });
}

