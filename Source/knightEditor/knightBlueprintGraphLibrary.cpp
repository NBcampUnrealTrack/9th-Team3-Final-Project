#include "knightBlueprintGraphLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	UEdGraph* FindGraph(UBlueprint* Blueprint, FName GraphName)
	{
		if (!Blueprint)
		{
			return nullptr;
		}
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		UEdGraph* const* Found = Graphs.FindByPredicate([GraphName](const UEdGraph* Graph) { return Graph && Graph->GetFName() == GraphName; });
		return Found ? *Found : nullptr;
	}
}

FString UknightBlueprintGraphLibrary::ExportGraphNodes(UBlueprint* Blueprint, FName GraphName)
{
	UEdGraph* const Graph = FindGraph(Blueprint, GraphName);
	if (!Graph)
	{
		return FString();
	}
	TSet<UObject*> Nodes;
	for (UEdGraphNode* const Node : Graph->Nodes)
	{
		if (Node)
		{
			Node->PrepareForCopying();
			Nodes.Add(Node);
		}
	}
	FString Text;
	FEdGraphUtilities::ExportNodesToText(Nodes, Text);
	return Text;
}

int32 UknightBlueprintGraphLibrary::ImportGraphNodes(UBlueprint* Blueprint, FName GraphName, const FString& Text, bool bReplace)
{
	UEdGraph* const Graph = FindGraph(Blueprint, GraphName);
	if (!Graph || !FEdGraphUtilities::CanImportNodesFromText(Graph, Text))
	{
		return -1;
	}
	if (bReplace)
	{
		for (UEdGraphNode* const Node : TArray<UEdGraphNode*>(Graph->Nodes))
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}
	TSet<UEdGraphNode*> Imported;
	FEdGraphUtilities::ImportNodesFromText(Graph, Text, Imported);
	for (UEdGraphNode* const Node : Imported)
	{
		// 덧붙일 때는 원본과 GUID가 겹칠 수 있으니 새로 발급한다(에디터 붙여넣기와 같은 처리).
		if (!bReplace)
		{
			Node->CreateNewGuid();
		}
		Node->PostPasteNode();
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return Imported.Num();
}

bool UknightBlueprintGraphLibrary::DuplicateMemberVariable(UBlueprint* Blueprint, FName Source, FName NewName)
{
	if (!Blueprint)
	{
		return false;
	}
	if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, NewName) != INDEX_NONE)
	{
		return true;
	}
	const int32 SourceIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Source);
	if (SourceIndex == INDEX_NONE)
	{
		return false;
	}
	const FEdGraphPinType PinType = Blueprint->NewVariables[SourceIndex].VarType;
	return FBlueprintEditorUtils::AddMemberVariable(Blueprint, NewName, PinType);
}
