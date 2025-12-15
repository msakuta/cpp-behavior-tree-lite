#include <iostream>
#include <string>
#include <vector>
#include "parser.h"

int main() {
	std::string src = "Hello world lorem ipsum some_thing\n";

	std::vector<std::string_view> tokens;

	std::string_view cursor = src;

	while (!cursor.empty()) {
		auto res = identifier(cursor);

		if (std::holds_alternative<std::string>(res)) {
			std::cout << "Error: " << std::get<std::string>(res) << "\n";
			break;
		}
		else {
			auto pair = std::get<Res>(res);
			print_res(pair);
			tokens.push_back(pair.second);
			cursor = pair.first;
		}
	}

	std::cout << "Tokens: " << tokens.size() << "\n";
	for (auto& token : tokens) {
		std::cout << token << "\n";
	}
	return 0;
}

