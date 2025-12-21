# cpp-behavior-tree-lite

This is a translated version of [rusty-behavior-tree-lite](https://github.com/msakuta/rusty-behavior-tree-lite) in C++17, which in turn inspired by [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP).

The main motivation of the series of behavior-tree-lite libraries is that BehaviorTreeCPP has grown huge and overkill for many use cases.
This library is dependency-free, single header-only library that you can copy & paste to your project.
We use a dedicated parser for a lightweight syntax to descibe the tree instead of the hefty XML files in BehaviorTreeCPP, which looks like this:

```
tree main = Sequence {
  PrintBodyNode
  Sequence {
    PrintArmNode (arm <- left_arm)
    PrintArmNode (arm <- right_arm)
  }
}
```

The [rusty-behavior-tree-lite](https://github.com/msakuta/rusty-behavior-tree-lite) library was quite nice, so I wanted to "backport" to C++, since many people still use C++ in robotics and gaming.

## Features TODO

* [ ] Subtrees
* [ ] Variables in tree definitions
* [ ] Static check on port types
* [ ] Type erasure on blackboard variables
* [ ] Shorthand `if` node notation
* [ ] Shorthand logical expression (`||`, `&&` and `!`)

## Examples

### Catchball example

You can run this example by:

```bash
$ make catchball && ./catchball
```

It will print an output like this indefinitely.
Hit Ctrl+C to escape.

```
| o             B |
| Ao            B |
| A o           B |
| A  o          B |
| A   o         B |
| A    o        B |
| A     o       B |
| A      o      B |
| A       o     B |
| A        o    B |
| A         o   B |
| A          o  B |
| A           o B |
| A            oB |
| A             o |
| A            oB |
| A           o B |
| A          o  B |
| A         o   B |
| A        o    B |
| A       o     B |
| A      o      B |
| A     o       B |
| A    o        B |
| A   o         B |
| A  o          B |
| A o           B |
| Ao            B |
| o             B |
| Ao            B |
| A o           B |
| A  o          B |
| A   o         B |
| A    o        B |
| A     o       B |
| A      o      B |
| A       o     B |
| A        o    B |
```

This example consists of 2 agents and 2 behavior trees.
Both trees share the same structure, but they retain states independently.
By ticking each of the tree every time, they can make progress in parallel.

The tree structure is like below.

```
tree main = Sequence {
    CatchBall(position <- position)
    ThrowBall(position <- position, speed <- speed)
}
```

The `CatchBall` node waits for the ball to arrive at the position given by the argument.
It will return `BehaviorResult::Running` until it receives the ball.

```c++
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
```

The `ThrowBall` node immediately throws the ball towards the direction and speed given by `speed` argument.

```c++
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
```

By repeating these nodes, The agents can throw the ball back and forth between them.
