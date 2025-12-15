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

void test_tree() {
	std::string src = R"(tree main = Sequence {
	Print
	})";

	auto res = source_text(src);

	if (auto e = std::get_if<1>(&res)) {
		std::cout << "Error: " << *e << "\n";
		return;
	}

	auto pair = std::get<0>(res);
	std::cout << "Tree parsed: " << pair.second << ", remainder: \"" << pair.first << "\"\n";
}

int main() {
	test_tree();
	return 0;
}

