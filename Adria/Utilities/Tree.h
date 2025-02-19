#pragma once

namespace adria
{
	template<typename T, typename Allocator>
	class Tree;

	template<typename T, typename Allocator>
	class TreeNode 
	{
		friend class Tree<T, Allocator>;
		using ChildrenList = std::vector<TreeNode*, typename std::allocator_traits<Allocator>::template rebind_alloc<TreeNode<T, Allocator>*>>;
		using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<TreeNode<T, Allocator>>;

	public:
		TreeNode(std::string const& node_name, T const& node_data,
			NodeAllocator* alloc,
			TreeNode* parent_node = nullptr)
			: name(node_name),
			data(node_data),
			parent(parent_node),
			children(alloc),
			allocator_ref(alloc) 
		{}
		~TreeNode() = default;

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
			TreeNode* child = std::allocator_traits<NodeAllocator>::allocate(*allocator_ref, 1);
			std::allocator_traits<NodeAllocator>::construct(*allocator_ref, child, child_name, child_data, allocator_ref, this);
			children.push_back(child);
			return child;
		}
		template<typename... Args>
		TreeNode* EmplaceChild(std::string const& child_name, Args&&... args)
		{
			T child_data(std::forward<Args>(args)...);
			TreeNode* child = std::allocator_traits<NodeAllocator>::allocate(*allocator_ref, 1);
			std::allocator_traits<NodeAllocator>::construct(*allocator_ref, child, child_name, child_data, allocator_ref, this);
			children.push_back(child);
			return child;
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
		ChildrenList children;
		NodeAllocator* allocator_ref;
	};

	template<typename T, typename Allocator>
	class Tree 
	{
		using NodeType = TreeNode<T, Allocator>;
		using NodeAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<NodeType>;
	public:

		explicit Tree(Allocator const& alloc = Allocator()) : node_allocator(alloc)
		{
		}

		Tree(std::string const& root_name, T const& root_data, Allocator const& alloc = Allocator())
			: node_allocator(alloc)
		{
			root = std::allocator_traits<NodeAllocator>::allocate(node_allocator, 1);
			std::allocator_traits<NodeAllocator>::construct(node_allocator, root, root_name, root_data, &node_allocator, nullptr);
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
				std::allocator_traits<NodeAllocator>::destroy(node_allocator, node);
				std::allocator_traits<NodeAllocator>::deallocate(node_allocator, node, 1);
				});
			root = nullptr;
		}

		void SetRoot(std::string const& root_name, T const& root_data)
		{
			Clear(); 
			root = std::allocator_traits<NodeAllocator>::allocate(node_allocator, 1);
			std::allocator_traits<NodeAllocator>::construct(node_allocator, root, root_name, root_data, &node_allocator, nullptr);
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
			TraversePreOrder([&count](TreeNode<T>*) { count++; });
			return count;
		}

		int Height() const 
		{
			return CalculateHeight(root.get());
		}

	private:
		NodeType* root;
		NodeAllocator node_allocator;

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

		Uint32 CalculateHeight(NodeType* node) const
		{
			if (!node) return -1;

			Uint32 max_child_height = -1;
			for (auto const& child : node->GetChildren()) 
			{
				int child_height = CalculateHeight(child.get());
				max_child_height = std::max(max_child_height, child_height);
			}
			return max_child_height + 1;
		}
	};
}