#pragma once
#include "AllocatorUtil.h"

namespace adria
{
	template<typename T, typename Allocator>
	class Tree;

	template<typename T, typename Allocator> requires IsAllocator<Allocator, T>
	class TreeNode 
	{
		friend class Tree<T, Allocator>;
		using ChildrenList = std::vector<TreeNode*>;
		using NodeAllocator = Allocator;

	public:
		TreeNode(std::string const& node_name, T const& node_data,
			Allocator& alloc, TreeNode* parent_node = nullptr)
			: name(node_name),
			data(node_data),
			parent(parent_node),
			children(),
			allocator_ref(alloc)
		{
		}
		~TreeNode()
		{
			for (auto& child : children)
			{
				child->~TreeNode();
				allocator_ref.Deallocate(child, 1);
			}
		}

		TreeNode* GetParent() const { return parent; }
		std::string_view GetName() const { return name; }
		T const& GetData() const { return data; }
		T& GetData() { return data; }
		void SetData(T const& _data)
		{
			data = _data;
		}
		void SetData(T&& _data)
		{
			data = std::move(_data);
		}
		template<typename... Args>
		void EmplaceData(Args&&... args)
		{
			data = T(std::forward<Args>(args)...);
		}

		TreeNode* AddChild(std::string const& child_name, T const& child_data)
		{
			TreeNode* child = allocator_ref.template Allocate<TreeNode<T, Allocator>>(1);
			new (child) TreeNode(child_name, child_data, allocator_ref, this);
			children.push_back(child);
			return child;
		}

		template<typename... Args>
		TreeNode* EmplaceChild(std::string const& child_name, Args&&... args)
		{
			T child_data(std::forward<Args>(args)...);
			return AddChild(child_name, child_data);
		}

		TreeNode* GetChild(Uint64 index) const 
		{
			if (index < children.size()) 
			{
				return children[index].get();
			}
			return nullptr;
		}

		auto const& GetChildren() const 
		{
			return children;
		}
		Uint64 GetChildCount() const
		{
			return children.size();
		}

		Bool RemoveChild(Uint64 index) 
		{
			if (index < children.size()) 
			{
				children.erase(children.begin() + index);
				return true;
			}
			return false;
		}

		Bool RemoveChild(TreeNode* child)
		{
			auto it = std::find_if(children.begin(), children.end(),
				[child](auto const& c) { return c.get() == child; });
			if (it != children.end()) 
			{
				children.erase(it);
				return true;
			}
			return false;
		}

		Bool IsLeaf() const
		{
			return children.empty();
		}

		Uint32 GetDepth() const 
		{
			Uint32 depth = 0;
			TreeNode* current = parent;
			while (current) 
			{
				depth++;
				current = current->parent;
			}
			return depth;
		}

		TreeNode* FindChild(std::string const& child_name) const 
		{
			for (auto const& child : children) 
			{
				if (child->name == child_name) 
				{
					return child.get();
				}
			}
			return nullptr;
		}

		TreeNode* FindDescendant(std::string const& node_name) const 
		{
			TreeNode* found = FindChild(node_name);
			if (found != nullptr) return found;

			for (auto const& child : children)
			{
				found = child->FindDescendant(node_name);
				if (found) return found;
			}
			return nullptr;
		}

	private:
		TreeNode* parent;
		std::string name;
		T data;
		std::vector<TreeNode*> children;
		Allocator& allocator_ref;
	};

	template<typename T, typename Allocator>
	class Tree 
	{
	public:
		using NodeType = TreeNode<T, Allocator>;
	public:
		explicit Tree(Allocator& allocator) : node_allocator(allocator), root(nullptr) {}
		Tree(std::string const& root_name, T const& root_data, Allocator& alloc)
			: node_allocator(alloc)
		{
			root = node_allocator.template Allocate<NodeType>(1);
			new (root) NodeType(root_name, root_data, node_allocator, nullptr);
		}

		~Tree()
		{
			Clear();
		}

		void Clear()
		{
			if (!root) return;

			TraversePostOrder([this](NodeType* node)
				{
					node->~TreeNode();
					node_allocator.Deallocate(node, 1);
				});
			root = nullptr;
		}

		void SetRoot(std::string const& root_name, T const& root_data)
		{
			Clear(); 
			root = node_allocator.template Allocate<NodeType>(1);
			new (root) NodeType(root_name, root_data, node_allocator, nullptr);
		}
		template<typename... Args>
		void EmplaceRoot(std::string const& root_name, Args&&... args)
		{
			T root_data(std::forward<Args>(args)...);
			SetRoot(root_name, root_data);
		}

		NodeType* GetRoot() const
		{
			return root;
		}

		NodeType* FindNode(std::string const& node_name) const
		{
			if (root->GetName() == node_name)
			{
				return root;
			}
			return root->FindDescendant(node_name);
		}

		void TraversePreOrder(std::function<void(NodeType*)> const& callback) const
		{
			TraversePreOrderImpl(root, callback);
		}

		void TraversePostOrder(std::function<void(NodeType*)> const& callback) const
		{
			TraversePostOrderImpl(root, callback);
		}

		Uint64 Size() const 
		{
			Uint64 count = 0;
			TraversePreOrder([&count](NodeType*) { count++; });
			return count;
		}

		Int32 Height() const
		{
			return CalculateHeight(root);
		}

	private:
		NodeType* root;
		Allocator& node_allocator;

	private:
		void TraversePreOrderImpl(NodeType* node, std::function<void(NodeType*)> const& callback) const
		{
			if (!node) return;

			callback(node);
			for (auto const& child : node->GetChildren()) 
			{
				TraversePreOrderImpl(child, callback);
			}
		}

		void TraversePostOrderImpl(NodeType* node, std::function<void(NodeType*)> const& callback) const
		{
			if (!node) return;

			for (auto const& child : node->GetChildren()) 
			{
				TraversePostOrderImpl(child, callback);
			}
			callback(node);
		}

		Int32 CalculateHeight(NodeType* node) const
		{
			if (!node) return -1;

			Int32 max_child_height = -1;
			for (auto const& child : node->GetChildren()) 
			{
				Int32 child_height = CalculateHeight(child.get());
				max_child_height = std::max(max_child_height, child_height);
			}
			return max_child_height + 1;
		}
	};
}