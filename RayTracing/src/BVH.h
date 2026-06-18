#pragma once

#include "Scene.h"
#include "Ray.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>
#include <algorithm>

/// Axis-aligned bounding box — two corners.
struct AABB
{
	glm::vec3 Min = glm::vec3(std::numeric_limits<float>::max());
	glm::vec3 Max = glm::vec3(std::numeric_limits<float>::lowest());

	void Expand(const glm::vec3& point)
	{
		Min = glm::min(Min, point);
		Max = glm::max(Max, point);
	}

	void Expand(const AABB& other)
	{
		Min = glm::min(Min, other.Min);
		Max = glm::max(Max, other.Max);
	}

	[[nodiscard]] glm::vec3 Centroid() const
	{
		return (Min + Max) * 0.5f;
	}

	[[nodiscard]] float SurfaceArea() const
	{
		const glm::vec3 d = Max - Min;
		return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	/// Ray-AABB intersection (slab test). Returns true if hit.
	/// tMin/tMax are used as the ray interval; updated to tight AABB intersection.
	[[nodiscard]] bool Intersect(const Ray& ray, float& tMin, float& tMax) const
	{
		for (int a = 0; a < 3; a++)
		{
			const float invD = 1.0f / ray.Direction[a];
			float t0 = (Min[a] - ray.Origin[a]) * invD;
			float t1 = (Max[a] - ray.Origin[a]) * invD;
			if (invD < 0.0f)
				std::swap(t0, t1);
			tMin = t0 > tMin ? t0 : tMin;
			tMax = t1 < tMax ? t1 : tMax;
			if (tMax <= tMin)
				return false;
		}
		return true;
	}
};

/// BVH node — flattened binary tree in array.
/// Internal nodes: LeftFirst >= 0 = left child index, Count = right child index.
/// Leaf nodes:      LeftFirst <  0 = encoded first-sphere index (use EncodeLeaf / DecodeFirstSphere),
///                  Count = number of spheres in leaf (> 0).
struct BVHNode
{
	AABB Bounds;
	int LeftFirst = 0;  // child or encoded sphere index
	int Count = 0;      // right child or sphere count
	int Axis = 0;       // split axis (debug / traversal hint)

	[[nodiscard]] bool IsLeaf() const noexcept { return LeftFirst < 0; }

	static int EncodeFirstSphere(int index) noexcept { return ~index; }
	static int DecodeFirstSphere(int encoded) noexcept { return ~encoded; }
};

/// Top-down BVH builder using mid-point split along the longest axis.
class BVH
{
public:
	BVH() = default;

	/// Build BVH from scene spheres. Clears previous tree.
	void Build(const Scene& scene);

	/// Leaf sphere range: [firstSphere, firstSphere + count).
	struct LeafRange
	{
		int FirstSphere = 0;
		int Count = 0;
	};

	/// Traverse the BVH and return the closest hit, or Miss().
	/// Performs AABB culling via the node bounds, then ray-sphere intersection for leaves.
	template <typename FnIntersectSphere>
	[[nodiscard]] auto Trace(const Ray& ray, FnIntersectSphere&& intersectSphere) const
		-> std::pair<float /* hitDistance */, int /* sphereIndex */>
	{
		if (m_Nodes.empty())
			return { -1.0f, -1 };

		float resultDist = std::numeric_limits<float>::max();
		int resultSphere = -1;

		// Stack-based traversal (max 64 levels for a ~2^64 sphere scene)
		int stack[kBVHMaxStackDepth];
		int stackPtr = 0;
		stack[stackPtr++] = 0;  // root node

		while (stackPtr > 0)
		{
			const int nodeIdx = stack[--stackPtr];
			const BVHNode& node = m_Nodes[nodeIdx];

			float tMin = 0.0f;
			float tMax = resultDist;
			if (!node.Bounds.Intersect(ray, tMin, tMax))
				continue;

			if (node.IsLeaf())
			{
				const int first = BVHNode::DecodeFirstSphere(node.LeftFirst);
				const int count = node.Count;
				for (int i = 0; i < count; i++)
				{
					const auto [hitDist, sphereIdx] = intersectSphere(ray, first + i);
					if (hitDist > 0.0f && hitDist < resultDist)
					{
						resultDist = hitDist;
						resultSphere = sphereIdx;
					}
				}
			}
			else
			{
				// Push children (closer first based on centroid distance)
				const int left = node.LeftFirst;
				const int right = node.Count;

				const float distLeft = glm::distance(ray.Origin, m_Nodes[left].Bounds.Centroid());
				const float distRight = glm::distance(ray.Origin, m_Nodes[right].Bounds.Centroid());

				if (distLeft < distRight)
				{
					stack[stackPtr++] = right;
					stack[stackPtr++] = left;
				}
				else
				{
					stack[stackPtr++] = left;
					stack[stackPtr++] = right;
				}
			}
		}

		return { resultDist, resultSphere };
	}

	[[nodiscard]] const std::vector<BVHNode>& Nodes() const noexcept { return m_Nodes; }
	[[nodiscard]] const std::vector<int>& SphereIndices() const noexcept { return m_SphereIndices; }
	[[nodiscard]] bool IsEmpty() const noexcept { return m_Nodes.empty(); }

private:
	int BuildRecursive(const Scene& scene, std::vector<int>& sphereIndices, int begin, int end);

	std::vector<BVHNode> m_Nodes;
	std::vector<int>    m_SphereIndices;
};
