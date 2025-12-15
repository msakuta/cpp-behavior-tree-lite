# cpp-behavior-tree-lite

This is a translated version of [rusty-behavior-tree-lite](https://github.com/msakuta/rusty-behavior-tree-lite) in C++17, which in turn inspired by [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP).

The main motivation of the series of behavior-tree-lite libraries is that BehaviorTreeCPP has grown huge and overkill for many use cases. In particular, we should not need the hefty XML files for every use case.
We use a dedicated parser for a lightweight syntax to descibe the tree, which looks like this: 

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
