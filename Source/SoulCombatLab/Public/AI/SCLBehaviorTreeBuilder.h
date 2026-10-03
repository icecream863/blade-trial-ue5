#pragma once

class UBehaviorTree;
class UObject;

// 在运行时构建敌人 Behavior Tree，供 AI Controller 初始化时使用。
namespace SCLBehaviorTreeBuilder
{
	SOULCOMBATLAB_API UBehaviorTree* BuildEnemyBehaviorTree(UObject& Outer);
}
