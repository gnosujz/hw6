#include "Geometry.h"
#include "RTree.h"
#include"assert.h"

namespace hw6 {

	// RNode 实现
	void RNode::add(RNode* child) {
		/*
		children[childrenNum] = child;
		child->parent = this;
		++childrenNum;*/
		if (!child) return;
		if (child->parent && child->parent != this) child->parent->remove(child);
		children.push_back(child);
		child->parent = this;
		++childrenNum;
		recalcBBox();
	}

	void RNode::remove(const Feature& f) {
		auto where = [&]() {
			for (auto itr = features.begin(); itr != features.end(); ++itr)
				if (itr->getName() == f.getName())
					return itr;
			}();
		features.erase(where);
		if (features.empty())
			features.shrink_to_fit();
			/*features.erase(where);
			if (features.empty())
				features.shrink_to_fit(); // free memory unused but allocated*/
	}

	void RNode::remove(RNode* child) {
		
		for (int i = 0; i < childrenNum; ++i)
			if (children[i] == child) {
				--childrenNum;
				std::swap(children[i], children[childrenNum]);
				children[childrenNum] = nullptr;
				break;
			}
		recalcBBox();
		/*if (!child) return;

		auto it = std::find(children.begin(), children.end(), child);
		if (it == children.end()) return;       // 子节点不在此节点中
		if (it != children.end() - 1) std::iter_swap(it, children.end() - 1);
		children.pop_back();
		if (childrenNum > 0) --childrenNum;
		child->parent = nullptr;
		recalcBBox();*/
	}

	Feature RNode::popBackFeature() {
		auto ret = features.back();
		features.pop_back();
		return ret;
	}

	RNode* RNode::popBackChildNode() {
		--childrenNum;
		auto ret = children[childrenNum];
		children[childrenNum] = nullptr;
		return ret;
		/*if (childrenNum == 0) return nullptr;
		--childrenNum;
		assert(childrenNum == static_cast<int>(children.size()));
		RNode* ret = children.back();
		children.pop_back();
		if (ret) ret->parent = nullptr;
		return ret;*/
	}

	void RNode::countNode(int& interiorNum, int& leafNum) {
		if (isLeafNode()) {
			++leafNum;
		}
		else {
			++interiorNum;
			for (int i = 0; i < childrenNum; ++i)
				children[i]->countNode(interiorNum, leafNum);
				assert(childrenNum == static_cast<int>(children.size()));
			/*for (auto* c : children)
				if (c)
					c->countNode(interiorNum, leafNum);*/
		}
	}

	int RNode::countHeight(int height) {
		++height;
		if (!isLeafNode()) {
			int cur = height;
			for (int i = 0; i < childrenNum; ++i)
				height = std::max(height, children[i]->countHeight(cur));
		}
		return height;
	}

	void RNode::draw() {
		if (isLeafNode()) {
			bbox.draw();
		}
		else
			for (int i = 0; i < childrenNum; ++i)
				children[i]->draw();
				assert(childrenNum == static_cast<int>(children.size()));
	}

	void RNode::rangeQuery(const Envelope& rect, std::vector<Feature>& result) {
		// Task rangeQuery
		/* TODO */
		if (!bbox.intersect(rect)) return;
		if (isLeafNode()) {
			for (const auto& f : features) {
				if (f.getEnvelope().intersect(rect)) result.push_back(f);
			}
		}
		else {
			for (int i = 0; i < childrenNum; ++i) {
				RNode* c = children[i];
				if (c) c->rangeQuery(rect, result);
				assert(childrenNum == static_cast<int>(children.size()));
			}
		}
		
		// filter step (选择查询区域与几何对象包围盒相交的几何对象)
		// 注意R树区域查询仅返回候选集，精炼步在hw6的rangeQuery中完成
	}

	RNode* RNode::pointInLeafNode(double x, double y) {
		// Task pointInLeafNode
		/* TODO */
		if (isLeafNode()) return this;
		if (children.empty()) return nullptr;
		Envelope pEnv(x, x, y, y); // 点的包围盒
		double bestInc = std::numeric_limits<double>::infinity();
		double bestArea = std::numeric_limits<double>::infinity();
		int bestIdx = -1;

		for (size_t i = 0; i < childrenNum; ++i) {
			RNode* c = children[i];
			if (!c) continue;
			// 计算将点包含进 child 的增加量（或直接使用 unionArea - area）
			double inc = enlargementToInclude(c->getEnvelope(), pEnv);
			double area = envelopeArea(c->getEnvelope());
			if (inc < bestInc || (inc == bestInc && area < bestArea)) {
				bestInc = inc;
				bestArea = area;
				bestIdx = static_cast<int>(i);
			}
		}

		if (bestIdx < 0) return nullptr;
		return children[bestIdx]->pointInLeafNode(x, y);
	}

	//Rnode计算包围盒
	void RNode::recalcBBox() {
		if (isLeafNode()) {
			if (features.empty()) { bbox = Envelope(); return; }
			Envelope e = features[0].getEnvelope();
			for (size_t i = 1; i < features.size(); ++i) e = e.unionEnvelope(features[i].getEnvelope());
			bbox = e;
		}
		else {
			if (children.empty()) { bbox = Envelope(); return; }
			Envelope e = children[0]->getEnvelope();
			for (size_t i = 1; i < childrenNum; ++i) e = e.unionEnvelope(children[i]->getEnvelope());
			assert(childrenNum == static_cast<int>(children.size()));
			bbox = e;
		}
	}

	// RTree 实现
	RTree::RTree(int maxChildren) : Tree(maxChildren), maxChildren(maxChildren) {
		if (maxChildren < 4) throw std::invalid_argument("maxChildren must be >= 4");
	}

	void RTree::countNode(int& interiorNum, int& leafNum) {
		interiorNum = leafNum = 0;
		if (root != nullptr)
			root->countNode(interiorNum, leafNum);
	}

	void RTree::countHeight(int& height) {
		height = 0;
		if (root != nullptr)
			height = root->countHeight(height);
	}

	//不明白为什么是40%
	static int MIN_CHILDREN_FROM_MAX(int maxChildren) {
		return std::max(1, (int)std::ceil(maxChildren * 0.4)); // 40% 最小填充
	}
	//删除递归节点
	static void deleteSubtree(RNode* node) {
		if (!node) return;
		if (!node->isLeafNode()) {
			int n = node->getChildNum();
			for (int i = 0; i < n; ++i) {
				RNode* c = node->getChildNode(i);
				deleteSubtree(c);
			}
		}
		delete node;

	}

	RNode* RTree::chooseleaf(RNode* node, const Envelope& box) {
		if (node->isLeafNode())
			return node;
		double bestInc = std::numeric_limits<double>::infinity();
		double bestArea = std::numeric_limits<double>::infinity();
		int bestIndex = -1;
		for (int i = 0; i < node->getChildNum(); ++i) {
			RNode* child = node->getChildNode(i);
			double inc = child->getEnvelope().unionEnvelope(box).getArea() - child->getEnvelope().getArea();
			double area = child->getEnvelope().getArea();
			if (inc < bestInc || (inc == bestInc && area < bestArea)) {
				bestInc = inc;
				bestArea = area;
				bestIndex = i;
			}
		}
		if (bestIndex < 0) return node;
		return chooseleaf(node->getChildNode(bestIndex), box);
	}

	void RTree::insertFeature(const Feature& f) {
		// 在 insertFeature(const Feature& f) 开头
		if (!root) {
			root = new RNode(f.getEnvelope());
			root->add(f);
			return;
		}
		RNode* leaf = chooseleaf(root, f.getEnvelope());
		leaf->add(f);
		if ((int)leaf->getFeatureNum() > maxChildren) {
			RNode* newNode = leaf->splitNode(leaf);
			updateTree(leaf, newNode);
		}
		else {
			// update ancestors' bbox
			RNode* cur = leaf;
			while (cur) { cur->recalcBBox(); cur = cur->getParent(); 
			}
		}
	}

	RNode* RNode::splitNode(RNode* node) {
		int minChildren = MIN_CHILDREN_FROM_MAX(maxChildren);
		if (node->isLeafNode()) {
			// operate on features
			int N = static_cast<int>(node->getFeatureNum());
			if (N <= 1) return nullptr;
			//if (N < 2) return new RNode(Envelope()); // 或直接返回 nullptr/不分裂
			std::vector<bool> assigned(N, false);
			// indices
			int seed1 = -1, seed2 = -1;
			double worstWaste = -std::numeric_limits<double>::infinity();
			for (int i = 0; i < N; ++i) {
				for (int j = i + 1; j < N; ++j) {
					Envelope ui = node->getFeature(i).getEnvelope().unionEnvelope(node->getFeature(j).getEnvelope());
					double waste = envelopeArea(ui) - envelopeArea(node->getFeature(i).getEnvelope()) - envelopeArea(node->getFeature(j).getEnvelope());
					if (waste > worstWaste) { worstWaste = waste; seed1 = i; seed2 = j; }
				}
			}
			if (seed1 < 0 || seed2 < 0) {
				// fallback: assign first to group1, second to group2 if possible
				seed1 = 0; seed2 = 1;
			}
			// 如果 N==1 或 seed 未找到，直接创建新节点并移动一项
			RNode* group2 = new RNode(Envelope());
			//group2->parent = node->parent;//=============

			// initialize group1 as node (we will rebuild both)
			std::vector<Feature> g1_feats; std::vector<Feature> g2_feats;
			g1_feats.push_back(node->getFeature(seed1)); assigned[seed1] = true;
			g2_feats.push_back(node->getFeature(seed2)); assigned[seed2] = true;
			Envelope g1_box = g1_feats[0].getEnvelope();
			Envelope g2_box = g2_feats[0].getEnvelope();
			int remain = N - 2;
			while (remain > 0) {
				// forced assignment to satisfy minChildren
				int need1 = std::max(0, minChildren - (int)g1_feats.size());
				int need2 = std::max(0, minChildren - (int)g2_feats.size());
				if (need1 == remain) {
					for (int i = 0; i < N; ++i) if (!assigned[i]) { g1_feats.push_back(node->getFeature(i)); assigned[i] = true; --remain; }
					break;
				}
				if (need2 == remain) {
					for (int i = 0; i < N; ++i) if (!assigned[i]) { g2_feats.push_back(node->getFeature(i)); assigned[i] = true; --remain; }
					break;
				}

				// pick entry with greatest difference in enlargement
				double bestDiff = -1.0; int bestIdx = -1; bool assignToG1 = true;
				for (int i = 0; i < N; ++i) if (!assigned[i]) {
					Envelope e = node->getFeature(i).getEnvelope();
					double inc1 = enlargementToInclude(g1_box, e);
					double inc2 = enlargementToInclude(g2_box, e);
					double diff = std::abs(inc1 - inc2);
					if (diff > bestDiff) { bestDiff = diff; bestIdx = i; assignToG1 = inc1 < inc2; }
					else if (diff == bestDiff) {
						double area1 = envelopeArea(g1_box), area2 = envelopeArea(g2_box);
						if (area1 < area2) assignToG1 = true; else assignToG1 = false;
					}
				}
				if (bestIdx < 0) { // 防御：若未找到，随便取一个未分配的
					for (int i = 0; i < N; ++i) if (!assigned[i]) { bestIdx = i; break; }
					if (bestIdx < 0) break;
				}
				// assign
				if (assignToG1) {
					g1_feats.push_back(node->getFeature(bestIdx));
					g1_box = g1_box.unionEnvelope(node->getFeature(bestIdx).getEnvelope());
				}
				else {
					g2_feats.push_back(node->getFeature(bestIdx));
					g2_box = g2_box.unionEnvelope(node->getFeature(bestIdx).getEnvelope());
				}
				assigned[bestIdx] = true;
				--remain;
			}

			// replace node's features with g1, create group2 with g2
			node->features = std::move(g1_feats);
			node->recalcBBox();
			group2->features = std::move(g2_feats);
			group2->recalcBBox();
			assert(N == static_cast<int>(node->getFeatureNum()) + static_cast<int>(group2->getFeatureNum()));
			//group2->parent = node->parent;
			return group2;
		}
		else {
			// internal node: split children vector
			//int N = static_cast<int>(node->children.size());
			int N = node->childrenNum;
			if (N <= 1) return nullptr;
			std::vector<bool> assigned(N, false);
			int seed1 = -1, seed2 = -1;
			double worstWaste = -std::numeric_limits<double>::infinity();
			for (int i = 0; i < N; ++i) for (int j = i + 1; j < N; ++j) {
				Envelope ui = node->children[i]->getEnvelope().unionEnvelope(node->children[j]->getEnvelope());
				double waste = envelopeArea(ui) - envelopeArea(node->children[i]->getEnvelope()) - envelopeArea(node->children[j]->getEnvelope());
				if (waste > worstWaste) { worstWaste = waste; seed1 = i; seed2 = j; }
			}
			if (seed1 < 0 || seed2 < 0) {
				// fallback: assign first to group1, second to group2 if possible
				seed1 = 0; seed2 = 1;
			}
			RNode* group2 = new RNode(Envelope());
			//group2->parent = node->parent;//==================
			std::vector<RNode*> g1_children; std::vector<RNode*> g2_children;
			g1_children.push_back(node->children[seed1]); assigned[seed1] = true;
			g2_children.push_back(node->children[seed2]); assigned[seed2] = true;
			Envelope g1_box = g1_children[0]->getEnvelope();
			Envelope g2_box = g2_children[0]->getEnvelope();
			int remain = N - 2;
			while (remain > 0) {
				int need1 = std::max(0, minChildren - (int)g1_children.size());
				int need2 = std::max(0, minChildren - (int)g2_children.size());
				if (need1 == remain) {
					for (int i = 0; i < N; ++i) if (!assigned[i]) { g1_children.push_back(node->children[i]); assigned[i] = true; --remain; }
					break;
				}
				if (need2 == remain) {
					for (int i = 0; i < N; ++i) if (!assigned[i]) { g2_children.push_back(node->children[i]); assigned[i] = true; --remain; }
					break;
				}
				double bestDiff = -1.0; int bestIdx = -1; bool assignToG1 = true;
				for (int i = 0; i < N; ++i) if (!assigned[i]) {
					Envelope e = node->children[i]->getEnvelope();
					double inc1 = enlargementToInclude(g1_box, e);
					double inc2 = enlargementToInclude(g2_box, e);
					double diff = std::abs(inc1 - inc2);
					if (diff > bestDiff) { bestDiff = diff; bestIdx = i; assignToG1 = inc1 < inc2; }
					else if (diff == bestDiff) {
						double area1 = envelopeArea(g1_box), area2 = envelopeArea(g2_box);
						assignToG1 = (area1 < area2);
					}
				}

				if (bestIdx < 0) {
					for (int i = 0; i < N; ++i) if (!assigned[i]) { bestIdx = i; break; }
					if (bestIdx < 0) break;
				}
				if (assignToG1) {
					g1_children.push_back(node->children[bestIdx]);
					g1_box = g1_box.unionEnvelope(node->children[bestIdx]->getEnvelope());
				}
				else {
					g2_children.push_back(node->children[bestIdx]);
					g2_box = g2_box.unionEnvelope(node->children[bestIdx]->getEnvelope());
				}
				assigned[bestIdx] = true;
				--remain;
			}
			// set node children = g1_children, group2 children = g2_children
			node->children = std::move(g1_children);
			node->childrenNum = static_cast<int>(node->children.size());
			for (int i = 0; i < node->childrenNum; ++i) if (node->children[i]) node->children[i]->parent = node;
			node->recalcBBox();
			//node->recalcBBox();
			group2->children = std::move(g2_children);
			group2->childrenNum = static_cast<int>(group2->children.size());
			// fix parent pointers

			for (int i = 0; i < group2->childrenNum; ++i) if (group2->children[i]) group2->children[i]->parent = group2;

			//group2->parent = node->parent;

			group2->recalcBBox();
			return group2;
		}
	}

	void RTree::updateTree(RNode* n, RNode* nn) {
		if (n == root) {
			// create new root
			Envelope newRootBox = n->getEnvelope().unionEnvelope(nn->getEnvelope());
			RNode* newRoot = new RNode(newRootBox);
			newRoot->add(n);
			newRoot->add(nn);
			// add 已设置 n->parent/nn->parent
			root = newRoot;
			return;
		}
		RNode* parent = n->getParent();
		if (!parent) {
			// 防御：若 parent 为空则把新节点提升为 root
			Envelope newRootBox = n->getEnvelope().unionEnvelope(nn->getEnvelope());
			RNode* newRoot = new RNode(newRootBox);
			newRoot->add(n);
			newRoot->add(nn);
			root = newRoot;
			return;
		}

		// 将 nn 加入 parent
		parent->add(nn);
		parent->recalcBBox();
		// 如果超出 capacity，则分裂 parent
		if (parent->getChildNum() > maxChildren) {
			RNode* newParent = parent->splitNode(parent);
			if (newParent) updateTree(parent, newParent);
		}
		else {
			RNode* cur = parent;
			while (cur) { cur->recalcBBox(); cur = cur->getParent(); }
		}
		if (root) root->recalcBBox();
		bbox = root ? root->getEnvelope() : hw6::Envelope();
	}

	bool RTree::constructTree(const std::vector<Feature>& features) {
		// Task RTree construction
		/* TODO
		构建可以采用几何特征按x轴的顺序逐个插入，
		基于节点新增面积越小越好的原则选择插入的节点，
		当超过节点所能存储的最大几何特征时，
		基于二次分裂(quadratic split)算法，
		选择最左和最右两个几何特征作为种子点，
		对几何特征进行分组。为了保持R-Tree的平衡性，
		建议随机选择几何特征插入，或类似AVL树，
		在插入后调整R-Tree结构。
		*/
		if (root) { deleteSubtree(root); root = nullptr; }

		if (features.empty()) { root = nullptr; return true; }

		// 可选随机化以获得更平衡树
		//std::vector<Feature> shuffled = features;
		//std::shuffle(shuffled.begin(), shuffled.end(),);

		for (const Feature& f : features) insertFeature(f);

		if (root) root->recalcBBox();

		bbox = root ? root->getEnvelope() : hw6::Envelope();

		return true;

	}

	void RTree::rangeQuery(const Envelope& rect, std::vector<Feature>& features) {
		features.clear();
		if (root != nullptr)
			root->rangeQuery(rect, features);
	}

	struct PQItem {
		RNode* node;
		double dist;
		PQItem(RNode* n = nullptr, double d = 0.0) : node(n), dist(d) {}
	};

	// 比较器：小的 dist 优先
	struct PQCmp { bool operator()(PQItem const& a, PQItem const& b) const { return a.dist > b.dist; } };

	// 返回点到包围盒的最短距离（下界），不是角点最大距
	static double envelopeDist(const Envelope& e, double x, double y) {
		double x1 = e.getMinX(), y1 = e.getMinY(), x2 = e.getMaxX(), y2 = e.getMaxY();
		double dx = 0.0, dy = 0.0;
		if (x < x1) dx = x1 - x;
		else if (x > x2) dx = x - x2;
		if (y < y1) dy = y1 - y;
		else if (y > y2) dy = y - y2;
		return std::hypot(dx, dy); // 最短欧氏距离到矩形
	}

	bool RTree::NNQuery(double x, double y, std::vector<Feature>& features) {
		printf("RTree::NNQuery start: using root env (%f,%f)-(%f,%f)\n",
			root->getEnvelope().getMinX(), root->getEnvelope().getMinY(),
			root->getEnvelope().getMaxX(), root->getEnvelope().getMaxY());

		features.clear();
		if (!root) return false;

		std::priority_queue<PQItem, std::vector<PQItem>, PQCmp> pq;
		double rootDist = envelopeDist(root->getEnvelope(), x, y);
		pq.push(PQItem(root, rootDist));
		printf("enqueue root dist=%f\n", rootDist);

		double bestDist = std::numeric_limits<double>::infinity();
		std::vector<Feature> candidates;

		while (!pq.empty()) {
			PQItem cur = pq.top(); pq.pop();
			if (!cur.node) continue;

			// 剪枝：当前节点下界距离 >= bestDist 则不可更优
			if (cur.dist >= bestDist) break;

			RNode* node = cur.node;
			if (node->isLeafNode()) {
				int fn = node->getFeatureNum();
				printf("leaf node features=%d\n", fn);
				for (int i = 0; i < fn; ++i) {
					const Feature& f = node->getFeature(i);
					printf("  leaf feature name=%s env=(%f,%f)-(%f,%f)\n",
						f.getName().c_str(),
						f.getEnvelope().getMinX(), f.getEnvelope().getMinY(),
						f.getEnvelope().getMaxX(), f.getEnvelope().getMaxY());
				}

				// 对叶内每个 Feature，用包围盒到点的最小距离作为下界 d_env
				for (int i = 0; i < fn; ++i) {
					const Feature& f = node->getFeature(i);
					double d_env = envelopeDist(f.getEnvelope(), x, y); // 包围盒到点的最小距离

					// 若下界 >= bestDist，则此 feature 及同 leaf 中更远的 feature 可剪掉
					if (d_env >= bestDist) continue;

					// 否则把 feature 加入候选，并更新 bestDist 为下界（越小越好）
					candidates.push_back(f);
					if (d_env < bestDist) bestDist = d_env;
				}
			}
			else {
				int cn = node->getChildNum();
				for (int i = 0; i < cn; ++i) {
					RNode* c = node->getChildNode(i);
					if (!c) continue;
					double d = envelopeDist(c->getEnvelope(), x, y);
					if (d < bestDist) {
						pq.push(PQItem(c, d));
						printf("enqueue child dist=%f\n", d);
					}
				}
			}
		}

		features = std::move(candidates);
		printf("RTree::NNQuery end: root env (%f,%f)-(%f,%f) found=%zu\n",
			root->getEnvelope().getMinX(), root->getEnvelope().getMinY(),
			root->getEnvelope().getMaxX(), root->getEnvelope().getMaxY(), features.size());

		return !features.empty();
	}

} // namespace hw6

