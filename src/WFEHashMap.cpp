#ifndef _WFE_HASH_MAP_H_
#define _WFE_HASH_MAP_H_

#include <atomic>

#include "DataNode.cpp"
#include "ArrayNode.cpp"
#include "utils.hpp"

#define MAX_FAIL_COUNT 10

using namespace std;

class WFEHashMap {
private:
	Node* head;
	const int keySize;

public:
	WFEHashMap(int _keySize): keySize(_keySize) {
		head = new ArrayNode(ARRAY_LENGTH);
	}

	bool put(char* key, int value) {
		char* hash = hashKey(key);
		Node* insertThis = allocateNode(value, hash);
		DEBUG("Node allocated");
		Node* local = head;

		for (int R = 0; R < keySize; R++) {
			DEBUG("Getting pos");
			int pos = (int) hash[R];
			DEBUG("Got pos");
			int failCount = 0;
			while (true) {
				if (failCount > MAX_FAIL_COUNT) {
					markDataNode(local, pos);
				}
				Node* node = getNode(local, pos);
				DEBUG("Got node");

				if (isArrayNode(node)) {
					DEBUG("is array node");
					local = node;
					break;
				} else if (isMarked(node)) {
					DEBUG("is marked node");
					local = expandTable(local, pos, node, R);
					break;
				} else if (node == nullptr) {
					DEBUG("is null");
					if (atomic_compare_exchange_weak(dynamic_cast<ArrayNode*>(local)->array + pos, &NULL_NODE, insertThis)) {
						DEBUG("Successful CAS");
						return true;
					} else {
						node = getNode(local, pos);
						if (isArrayNode(node)) {
							DEBUG("is array node: 2");
							local = node;
							break;
						} else if (isMarked(node)) {
							DEBUG("is marked node: 2");
							local = expandTable(local, pos, node, R);
							break;
						} else if (hashEqual(dynamic_cast<DataNode*>(node)->getKey(), dynamic_cast<DataNode*>(insertThis)->getKey(), keySize)) {
							DEBUG("Hashes are equal. Deleting insertThis");
							delete insertThis;
							return true;
						} else {
							DEBUG("fail");
							failCount++;
						}
					}
				} else {
					DEBUG("first else");
					if(hashEqual(dynamic_cast<DataNode*>(node)->getKey(), dynamic_cast<DataNode*>(insertThis)->getKey(), keySize)) {
						DEBUG("Hash equal");
						if (atomic_compare_exchange_weak(dynamic_cast<ArrayNode*>(local)->array + pos, &node, insertThis)) {
							DEBUG("CAS Successful. Deleting old node");
							delete node;
							return true;
						} else {
							DEBUG("CAS failed.");
							Node* node2 = getNode(local, pos);
							if (isArrayNode(node2)) {
								local = node2;
								break;
							} else if (isMarked(node2) ^ unmarkedEqual(node2, node, keySize)) {
								local = expandTable(local, pos, node, R);
								break;
							} else {
								delete insertThis;
								return true;
							}
						}
					} else {
						local = expandTable(local, pos, node, R);
						if (!isArrayNode(local)) {
							failCount++;
						} else {
							break;
						}
					}
				}
			}
		}

		return false;
	}
};

#endif
