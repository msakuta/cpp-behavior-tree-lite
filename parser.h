#include <string_view>
#include <variant>

struct Node {
};

using Res = std::pair<std::string_view, std::string_view>;

using IResult = std::variant<Res, std::string>;

IResult identifier(std::string_view i) {
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
	IResult ret = std::make_pair(r, i.substr(0, r.data() - i.data()));
	return ret;
}

void print_res(const Res& res) {
	std::cout << "(" << res.first << ", " << res.second << ")";
}

