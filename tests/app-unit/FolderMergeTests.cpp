// The rescan's merge (audit #307 PL-014): a file still on disk keeps the
// object every holder points at; only what vanished is handed over to be
// deleted; what is new moves in. A tree of plain nodes stands in for
// FolderData, and a "collection" of raw pointers for every holder the
// rescan used to leave dangling.
#include "doctest/doctest.h"
#include "FolderMerge.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace
{
	struct Node
	{
		std::string path;
		bool folder = false;
		Node* parent = nullptr;
		std::vector<Node*> kids;
	};

	struct Tree
	{
		std::set<Node*> deleted;
		std::vector<std::string> vanishedPaths, arrivedPaths;

		std::vector<Node*> children(Node* n) { return n->kids; }
		std::string key(Node* n) { return n->path; }
		bool isFolder(Node* n) { return n->folder; }
		void detach(Node* folder, Node* n)
		{
			folder->kids.erase(std::remove(folder->kids.begin(), folder->kids.end(), n), folder->kids.end());
			n->parent = nullptr;
		}
		void attach(Node* folder, Node* n) { folder->kids.push_back(n); n->parent = folder; }
		void destroy(Node* n)
		{
			for (Node* k : n->kids)
				destroy(k);
			deleted.insert(n);
			delete n;
		}
		void vanished(Node* n)
		{
			vanishedPaths.push_back(n->path);
			if (n->parent)
				detach(n->parent, n);
			destroy(n);
		}
		void arrived(Node* n) { arrivedPaths.push_back(n->path); }
	};

	Node* add(Node* folder, const std::string& name, bool isFolder = false)
	{
		Node* n = new Node();
		n->path = folder->path + "/" + name;
		n->folder = isFolder;
		n->parent = folder;
		folder->kids.push_back(n);
		return n;
	}

	Node* find(Node* folder, const std::string& path)
	{
		for (Node* k : folder->kids)
		{
			if (k->path == path)
				return k;
			if (k->folder)
				if (Node* f = find(k, path))
					return f;
		}
		return nullptr;
	}
}

TEST_CASE("rescan merge: an unchanged file keeps the object its holders point at")
{
	Tree tree;
	Node* live = new Node(); live->path = "/r"; live->folder = true;
	add(live, "a.nes");
	Node* b = add(live, "b.nes");
	Node* sub = add(live, "sub", true);
	Node* c = add(sub, "c.nes");
	// The holders: a collection's entry for b, a group folder's for sub, a
	// hasher's queue for c.
	std::vector<Node*> holders = { b, sub, c };

	// The folder as it is on disk now: a.nes gone, d.nes and sub/e.nes and
	// a new folder new/f.nes added, b.nes and sub/c.nes unchanged.
	Node* fresh = new Node(); fresh->path = "/r"; fresh->folder = true;
	add(fresh, "b.nes");
	Node* freshSub = add(fresh, "sub", true);
	add(freshSub, "c.nes");
	add(freshSub, "e.nes");
	add(fresh, "d.nes");
	Node* freshNew = add(fresh, "new", true);
	add(freshNew, "f.nes");

	FolderMerge::merge(live, fresh, tree);

	for (Node* h : holders)
		CHECK_MESSAGE(tree.deleted.count(h) == 0, "a holder's object was deleted by the rescan");
	CHECK(find(live, "/r/b.nes") == b);
	CHECK(find(live, "/r/sub") == sub);
	CHECK(find(live, "/r/sub/c.nes") == c);
	CHECK(find(live, "/r/a.nes") == nullptr);
	CHECK(find(live, "/r/d.nes") != nullptr);
	CHECK(find(live, "/r/sub/e.nes") != nullptr);
	CHECK(find(live, "/r/new/f.nes") != nullptr);
	CHECK(tree.vanishedPaths == std::vector<std::string>{ "/r/a.nes" });
	std::vector<std::string> arrived = tree.arrivedPaths;
	std::sort(arrived.begin(), arrived.end());
	CHECK(arrived == std::vector<std::string>{ "/r/d.nes", "/r/new", "/r/sub/e.nes" });

	// What is left in the fresh tree is duplicates only, never handed out.
	CHECK(find(fresh, "/r/d.nes") == nullptr);
	CHECK(find(fresh, "/r/b.nes") != nullptr);
	CHECK(find(fresh, "/r/b.nes") != b);

	tree.destroy(fresh);
	tree.destroy(live);
}

TEST_CASE("rescan merge: an entry that changed kind is replaced, not kept")
{
	Tree tree;
	Node* live = new Node(); live->path = "/r"; live->folder = true;
	add(live, "x", true);          // a folder in memory
	Node* fresh = new Node(); fresh->path = "/r"; fresh->folder = true;
	add(fresh, "x");               // a file of that name on disk now
	FolderMerge::merge(live, fresh, tree);
	CHECK(tree.vanishedPaths == std::vector<std::string>{ "/r/x" });
	CHECK(tree.arrivedPaths == std::vector<std::string>{ "/r/x" });
	REQUIRE(live->kids.size() == 1);
	CHECK_FALSE(live->kids[0]->folder);
	tree.destroy(fresh);
	tree.destroy(live);
}

TEST_CASE("rescan merge: nothing changed, nothing moves")
{
	Tree tree;
	Node* live = new Node(); live->path = "/r"; live->folder = true;
	Node* a = add(live, "a.nes");
	Node* fresh = new Node(); fresh->path = "/r"; fresh->folder = true;
	add(fresh, "a.nes");
	FolderMerge::merge(live, fresh, tree);
	CHECK(tree.vanishedPaths.empty());
	CHECK(tree.arrivedPaths.empty());
	CHECK(find(live, "/r/a.nes") == a);
	tree.destroy(fresh);
	tree.destroy(live);
}
