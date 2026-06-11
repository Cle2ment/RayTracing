#include "BVH.h"

#include <glm/gtc/type_ptr.hpp>

void BVH::Build(const Scene& scene)
{
	m_Nodes.clear();

	const size_t sphereCount = scene.Spheres.size();
	if (sphereCount == 0)
		return;

	// Build sorted index array
	std::vector<int> indices(sphereCount);
	for (size_t i = 0; i < sphereCount; i++)
		indices[i] = static_cast<int>(i);

	// Reserve node capacity (2 * sphereCount - 1 max)
	m_Nodes.reserve(sphereCount * 2);

	// Build root
	BuildRecursive(scene, indices, 0, static_cast<int>(sphereCount));

	// Save sorted sphere indices for GPU upload
	m_SphereIndices = std::move(indices);
}

int BVH::BuildRecursive(const Scene& scene, std::vector<int>& sphereIndices, int begin, int end)
{
	const int count = end - begin;

	// Compute bounding box
	AABB bounds;
	for (int i = begin; i < end; i++)
	{
		const int idx = sphereIndices[i];
		const Sphere& sphere = scene.Spheres[idx];
		const glm::vec3 rvec(sphere.Radius);
		bounds.Expand(sphere.Position - rvec);
		bounds.Expand(sphere.Position + rvec);
	}

	// Leaf node: small enough or single sphere
	if (count <= 2)
	{
		const int nodeIdx = static_cast<int>(m_Nodes.size());
		m_Nodes.emplace_back(BVHNode{ bounds, BVHNode::EncodeFirstSphere(begin), count, 0 });
		return nodeIdx;
	}

	// Find split axis (longest)
	int axis = 0;
	{
		const glm::vec3 extent = bounds.Max - bounds.Min;
		if (extent.y > extent.x) axis = 1;
		if (extent.z > extent[axis]) axis = 2;
	}

	// Sort by centroid along split axis
	std::vector<int> sorted(sphereIndices.begin() + begin, sphereIndices.begin() + end);
	std::sort(sorted.begin(), sorted.end(),
		[&scene, axis](int a, int b) {
			return scene.Spheres[a].Position[axis] < scene.Spheres[b].Position[axis];
		});

	// Copy sorted back and split at median
	for (int i = 0; i < count; i++)
		sphereIndices[begin + i] = sorted[i];

	const int mid = begin + count / 2;

	// Reserve node slot, then build children
	const int nodeIdx = static_cast<int>(m_Nodes.size());
	m_Nodes.push_back({});  // placeholder

	const int leftChild = BuildRecursive(scene, sphereIndices, begin, mid);
	const int rightChild = BuildRecursive(scene, sphereIndices, mid, end);

	m_Nodes[nodeIdx] = BVHNode{ bounds, leftChild, rightChild, axis };
	return nodeIdx;
}
