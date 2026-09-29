#pragma once
#ifndef ES_APP_FOLDER_MERGE_H
#define ES_APP_FOLDER_MERGE_H

// A folder re-read from disk, folded into the tree already in memory
// (audit #307 PL-014; SystemData::rescanIfFolderChanged).
//
// The rescan deleted every FileData of the system and read the folder
// again: clear(), then populateFolder. Every holder of one of those
// pointers -- the collections' entries (recently played, favorites, all
// games, the custom ones), a group system's folder for its grouped child,
// a hasher's queue -- then held freed memory, for files that had not
// changed at all. The view was one such holder, and was fixed (#246); the
// rest were not. So the rescan reads the folder into a tree of its own and
// merges: an entry still on disk keeps the object it had, an entry only on
// disk moves in, an entry gone from disk is handed to the caller, who drops
// every holder of it and deletes it. Only what vanished from disk is ever
// deleted, which is what a player deleting a game already does.
//
// Generic, so the rule has a test without a window: Tree supplies
//   std::vector<Node*> children(Node* folder)   a copy
//   std::string key(Node* n)                    its full path
//   bool isFolder(Node* n)
//   void detach(Node* folder, Node* n)          out of folder, not deleted
//   void attach(Node* folder, Node* n)          into folder, parent set
//   void vanished(Node* n)                      in memory, gone from disk
//   void arrived(Node* n)                       moved in from the fresh tree
// Afterwards `fresh` holds only duplicates of what `live` kept, never seen
// by anything, for the caller to delete.

#include <string>
#include <unordered_map>
#include <vector>

namespace FolderMerge
{
	template <class Node, class Tree>
	void merge(Node* live, Node* fresh, Tree& tree)
	{
		// What is on disk at this level, by path.
		std::unordered_map<std::string, Node*> onDisk;
		for (Node* f : tree.children(fresh))
			onDisk[tree.key(f)] = f;

		// Gone from disk, or a file that is a folder now (or the reverse):
		// the caller's to drop and delete. Everything else stays as it is.
		std::unordered_map<std::string, Node*> kept;
		for (Node* l : tree.children(live))
		{
			auto it = onDisk.find(tree.key(l));
			if (it == onDisk.end() || tree.isFolder(it->second) != tree.isFolder(l))
				tree.vanished(l);
			else
				kept[tree.key(l)] = l;
		}

		// New on disk: moved in whole, a new folder with everything in it.
		// A folder both sides have is merged the same way, one level down.
		for (Node* f : tree.children(fresh))
		{
			auto it = kept.find(tree.key(f));
			if (it == kept.end())
			{
				tree.detach(fresh, f);
				tree.attach(live, f);
				tree.arrived(f);
			}
			else if (tree.isFolder(f))
				merge(it->second, f, tree);
		}
	}
}

#endif // ES_APP_FOLDER_MERGE_H
