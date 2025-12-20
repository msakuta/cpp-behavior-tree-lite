//! An example program that demonstrates 2 agents throwing a ball
//! back and forth using a logic described in a behavior tree.
//! Each agent A and B has its own state and ticked in parallel.
#include "../behavior_tree_lite.h"
#include <thread>
#include <chrono>

// Note that the ball states are stored in global variables, which is not the best practice in production.
// We do this here for brevity.
// Consider putting them in a struct and exchange information to the behavior nodes by context variables.
int ball_pos = 1;
int ball_speed = 0;
constexpr int A_pos = 1;
constexpr int A_speed = 1;
constexpr int B_pos = 15;
constexpr int B_speed = -1;

/// Wait until a player receives a ball.
class CatchBall : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto position = std::atoi(context.get("position")->c_str());
        if (ball_pos == position) {
            return BehaviorResult::Success;
        }
        return BehaviorResult::Running;
    }
};

/// Throws a ball from current position with the given speed.
class ThrowBall : public BehaviorNode {
    BehaviorResult tick(Context& context) override {
        auto position = std::atoi(context.get("position")->c_str());
        if (ball_pos != position) {
            // You cannot throw a ball that is not in your hands.
            return BehaviorResult::Fail;
        }
        auto speed = std::atoi(context.get("speed")->c_str());
        ball_speed = speed;
        return BehaviorResult::Success;
    }
};

void print_ball() {
    std::cout << '|';
    for (size_t i = 0; i < 17; i++) {
        if (ball_pos == i) std::cout << 'o';
        else if (A_pos == i) std::cout << 'A';
        else if (B_pos == i) std::cout << 'B';
        else std::cout << ' ';
    }
    std::cout << '|' << std::endl; // Flush explicitly
}

void run() {
    std::string src = R"(tree main = Sequence {
    CatchBall(position <- position)
    ThrowBall(position <- position, speed <- speed)
}
)";
    auto res = source_text(src);

    if (auto e = std::get_if<1>(&res)) {
        std::cout << "Error: " << *e << "\n";
        return;
    }

    auto pair = std::get<0>(res);

    auto registry = defaultRegistry();
    registry.node_types.emplace(std::string("CatchBall"),
        std::function([](){ return static_cast<std::unique_ptr<BehaviorNode>>(
            std::make_unique<CatchBall>()); }));
    registry.node_types.emplace(std::string("ThrowBall"),
        std::function([](){ return static_cast<std::unique_ptr<BehaviorNode>>(
            std::make_unique<ThrowBall>()); }));

    auto player_A_tree = load(pair.second, registry);
    Blackboard player_A_bb = Blackboard{};
    player_A_bb["position"] = std::to_string(A_pos);
    player_A_bb["speed"] = std::to_string(A_speed);
    auto player_A_context = Context{ .blackboard = player_A_bb };

    auto player_B_tree = load(pair.second, registry);
    Blackboard player_B_bb = Blackboard{};
    player_B_bb["position"] = std::to_string(B_pos);
    player_B_bb["speed"] = std::to_string(B_speed);
    auto player_B_context = Context{ .blackboard = player_B_bb };

    BehaviorResult player_A_res = BehaviorResult::Success;
    BehaviorResult player_B_res = BehaviorResult::Success;
    do {
        ball_pos += ball_speed;
        print_ball();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        player_A_res = player_A_tree->tick(player_A_context);
        player_B_res = player_B_tree->tick(player_B_context);
    } while (player_A_res != BehaviorResult::Success || player_B_res != BehaviorResult::Success );
}

int main() {
    run();
    return 0;
}

