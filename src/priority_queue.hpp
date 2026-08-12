#ifndef SJTU_PRIORITY_QUEUE_HPP
#define SJTU_PRIORITY_QUEUE_HPP

#include <cstddef>
#include <functional>
#include "exceptions.hpp"

namespace sjtu {
/**
 * @brief a container like std::priority_queue which is a heap internal.
 * **Exception Safety**: The `Compare` operation might throw exceptions for certain data.
 * In such cases, any ongoing operation should be terminated, and the priority queue should
 * be restored to its original state before the operation began.
 *
 * Implementation notes:
 *  - Internally a leftist heap is used, so push / pop / merge all cost O(log n)
 *    (merge is therefore within the required O(log n) bound).
 *  - In mergeNodes() every call of the comparator happens in the descending
 *    pass, while every structural modification happens in the rebuilding pass.
 *    Consequently, if the comparator throws, no node pointer has been changed yet and
 *    both heaps are still intact, which provides the required strong exception safety.
 */
template<typename T, class Compare = std::less<T>>
class priority_queue {
private:
	struct Node {
		Node *left, *right;
		T data;
		int dist; // null path length (npl)
		Node(const T &d) : left(nullptr), right(nullptr), data(d), dist(1) {}
	};

	Node *root;
	size_t sz;
	Compare cmp;

	static int distOf(const Node *p) {
		return p ? p->dist : 0;
	}

	/**
	 * @brief merge two leftist heaps rooted at a and b (both owned by the caller).
	 * @return the root of the merged heap.
	 * @note all comparator invocations happen in the first (descending) pass, while
	 *       every pointer modification happens in the second (rebuilding) pass, so if
	 *       the comparator throws, the two trees remain completely untouched.
	 *       The merge path of a leftist heap has length O(log n), therefore the
	 *       operation runs in O(log n) time.
	 */
	Node *mergeNodes(Node *a, Node *b) {
		if (!a) return b;
		if (!b) return a;
		Node *path[128]; // the merge path never exceeds log2(size) nodes
		int top = 0;
		while (a && b) {
			if (cmp(a->data, b->data)) { // a has lower priority -> b becomes the root of this step
				Node *t = a; a = b; b = t;
			}
			path[top++] = a;
			a = a->right;
		}
		Node *carry = a ? a : b; // the remaining subtree (recursive base case)
		while (top > 0) {
			Node *n = path[--top];
			n->right = carry;
			if (distOf(n->left) < distOf(n->right)) {
				Node *t = n->left; n->left = n->right; n->right = t;
			}
			n->dist = distOf(n->right) + 1;
			carry = n;
		}
		return carry;
	}

	/**
	 * @brief iteratively destroy a whole tree (avoids deep recursion).
	 * @param r root of the tree to destroy.
	 * @param cap upper bound of the number of nodes in the tree.
	 */
	static void destroyTree(Node *r, size_t cap) {
		if (!r) return;
		Node **stk = new Node *[cap ? cap : 1];
		size_t top = 0;
		stk[top++] = r;
		while (top) {
			Node *p = stk[--top];
			if (p->left) stk[top++] = p->left;
			if (p->right) stk[top++] = p->right;
			delete p;
		}
		delete[] stk;
	}

	/**
	 * @brief iteratively deep-copy a whole tree (avoids deep recursion).
	 * @param r root of the tree to copy.
	 * @param cap number of nodes in the tree.
	 * @return root of the new tree.
	 */
	static Node *cloneTree(Node *r, size_t cap) {
		if (!r) return nullptr;
		Node **srcStk = nullptr;
		Node **dstStk = nullptr;
		Node *newRoot = nullptr;
		try {
			srcStk = new Node *[cap ? cap : 1];
			dstStk = new Node *[cap ? cap : 1];
			size_t top = 0;
			newRoot = new Node(r->data);
			srcStk[top] = r; dstStk[top] = newRoot; ++top;
			while (top) {
				--top;
				Node *s = srcStk[top];
				Node *d = dstStk[top];
				if (s->left) {
					d->left = new Node(s->left->data);
					srcStk[top] = s->left; dstStk[top] = d->left; ++top;
				}
				if (s->right) {
					d->right = new Node(s->right->data);
					srcStk[top] = s->right; dstStk[top] = d->right; ++top;
				}
			}
		} catch (...) {
			destroyTree(newRoot, cap);
			delete[] srcStk;
			delete[] dstStk;
			throw;
		}
		delete[] srcStk;
		delete[] dstStk;
		return newRoot;
	}

public:
	/**
	 * @brief default constructor
	 */
	priority_queue() : root(nullptr), sz(0), cmp() {}

	/**
	 * @brief copy constructor
	 * @param other the priority_queue to be copied
	 */
	priority_queue(const priority_queue &other) : root(nullptr), sz(other.sz), cmp(other.cmp) {
		root = cloneTree(other.root, other.sz);
	}

	/**
	 * @brief deconstructor
	 */
	~priority_queue() {
		destroyTree(root, sz);
	}

	/**
	 * @brief Assignment operator
	 * @param other the priority_queue to be assigned from
	 * @return a reference to this priority_queue after assignment
	 */
	priority_queue &operator=(const priority_queue &other) {
		if (this == &other) return *this;
		Node *newRoot = cloneTree(other.root, other.sz); // strong guarantee
		destroyTree(root, sz);
		root = newRoot;
		sz = other.sz;
		cmp = other.cmp;
		return *this;
	}

	/**
	 * @brief get the top element of the priority queue.
	 * @return a reference of the top element.
	 * @throws container_is_empty if empty() returns true
	 */
	const T & top() const {
		if (!root) throw container_is_empty();
		return root->data;
	}

	/**
	 * @brief push new element to the priority queue.
	 * @param e the element to be pushed
	 */
	void push(const T &e) {
		Node *node = new Node(e);
		try {
			root = mergeNodes(root, node);
		} catch (...) {
			delete node; // restore: the queue is unchanged, drop the new node
			throw runtime_error();
		}
		++sz;
	}

	/**
	 * @brief delete the top element from the priority queue.
	 * @throws container_is_empty if empty() returns true
	 */
	void pop() {
		if (!root) throw container_is_empty();
		Node *old = root;
		Node *merged;
		try {
			merged = mergeNodes(old->left, old->right);
		} catch (...) {
			throw runtime_error(); // restore: nothing was modified yet
		}
		root = merged;
		--sz;
		delete old;
	}

	/**
	 * @brief return the number of elements in the priority queue.
	 * @return the number of elements.
	 */
	size_t size() const {
		return sz;
	}

	/**
	 * @brief check if the container is empty.
	 * @return true if it is empty, false otherwise.
	 */
	bool empty() const {
		return sz == 0;
	}

	/**
	 * @brief merge another priority_queue into this one.
	 * The other priority_queue will be cleared after merging.
	 * The complexity is at most O(logn).
	 * @param other the priority_queue to be merged.
	 */
	void merge(priority_queue &other) {
		if (this == &other) return;
		Node *merged;
		try {
			merged = mergeNodes(root, other.root);
		} catch (...) {
			throw runtime_error(); // restore: both queues are unchanged
		}
		root = merged;
		sz += other.sz;
		other.root = nullptr;
		other.sz = 0;
	}
};

}

#endif
