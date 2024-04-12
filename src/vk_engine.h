// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_loader.h>

struct DeletionQueue {
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call functors
		}

		deletors.clear();
	}
};

struct FrameData {
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore, _renderSemaphore; // imageReady, renderFinished
	VkFence _renderFence;

	DeletionQueue _deletionQueue;
	DescriptorAllocatorGrowable _frameDescriptors; // at the moment, only GPUSceneData
};

// Global scene data. A buffer is created per frame.
struct GPUSceneData {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 viewproj;
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; // w for sun power
	glm::vec4 sunlightColor;
};

struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

// Builds pipelines and writes descriptors for materials
struct GLTFMetallic_Roughness {
	// Color and metal-rough factors
	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14]; // 256 Bytes is a good default alignment on most GPUs
	};

	// used to write to DescriptorSet. Init w/ default data at the moment.
	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer; // holds MaterialConstants data
		uint32_t dataBufferOffset;
	};

	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout; // layout for MaterialResources
	DescriptorWriter writer;

	void build_pipelines(VulkanEngine* engine);
	
	MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
};

// Appended into DrawContext by (Mesh)Nodes.Draw()
struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

// Objects to render per frame. Only opaques at the moment.
struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
};

struct MeshNode : public Node {
	std::shared_ptr<MeshAsset> mesh;

	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };

	VkExtent2D _windowExtent{ 1700 , 900 };

	VkDevice _device; // Vulkan device for commands
	DeletionQueue _mainDeletionQueue; // Should probably make an interface...

	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	GPUSceneData sceneData; // VP matrices and lighting (updated per frame)
	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout; // Just the layout. Descriptor set is per-frame

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	// upload the meshes into GPU
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

private:
	// Main Vulkan structs
	VkInstance _instance;// Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger;// Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;// GPU chosen as the default device
	VkSurfaceKHR _surface;// Vulkan window surface

	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	bool resize_requested = false;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;

	VmaAllocator _allocator;

	// Scene graph
	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;

	// For Compute Shader image and Materials
	DescriptorAllocatorGrowable globalDescriptorAllocator;

	// For compute shader to draw into
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;

	// Compute shader pipeline
	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// Default Textures
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	AllocatedImage _errorCheckerboardImage;

	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

	// Used to test textures
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	// Immediate submit structures (just blocks CPU until GPU finishes the commands)
	VkFence _immFence;
	VkCommandBuffer _immCommandBuffer;
	VkCommandPool _immCommandPool;

	// Imgui gradient
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	// GLTF
	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	// Test material
	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	void init_imgui();
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

	void init_vulkan();

	void init_swapchain();
	void create_swapchain(uint32_t width, uint32_t height);
	void resize_swapchain();
	void destroy_swapchain();

	void init_commands();
	void init_sync_structures();

	void init_descriptors();

	void init_pipelines();
	void init_background_pipelines();

	void init_default_data();

	void draw_background(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void draw_geometry(VkCommandBuffer cmd);

	void update_scene();

	FrameData& get_current_frame() { return _frames[_frameNumber % FRAME_OVERLAP]; };

	AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
	void destroy_image(const AllocatedImage& img);
};
