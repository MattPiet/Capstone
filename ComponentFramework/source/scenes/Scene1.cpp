#include <glew.h>
#include <iostream>
#include "core/Debug.h"
#include "scenes/Scene1.h"
#include <MMath.h>
#include <__msvc_ranges_to.hpp>

#include "physics/TransformComponent.h"
#include "render/VulkanRenderer.h"
#include "render/OpenGLRenderer.h"
#include "scenes/SceneManager.h"
#include <core/Profiling.h>

Scene1::Scene1(Renderer *renderer_): 
	Scene(nullptr),renderer(renderer_) {
	Debug::Info("Created Scene1: ", __FILE__, __LINE__);
}

Scene1::~Scene1() {
}

bool Scene1::OnCreate() {
	MonitorTimingStart("Scene1 OnCreate");
	int width = 0, height = 0;
	float aspectRatio;

	switch (renderer->getRendererType()){
	case RendererType::VULKAN:
	{
		VulkanRenderer* vRenderer;
		vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
			
			assetManager = std::make_shared<XMLAssetManager>();
			std::vector<std::string> names{ 
				"MarioFire",
				"MarioMime"
			};
			
			SDL_GetWindowSize(vRenderer->getWindow(), &width, &height);
			aspectRatio = static_cast<float>(width) / static_cast<float>(height);
			camera_actor_ = std::make_unique<CameraActor>(std::weak_ptr<Component>(), 45.0f, aspectRatio, 0.5f, 100.0f);
			camera_actor_->AddComponent(Ref<TransformComponent>() = std::make_shared<TransformComponent>(std::weak_ptr<Component>()));
			camera_actor_->SetView(Quaternion(), Vec3(0.0f,0.0f,5.0f));
		
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(vRenderer->getPhysicalDevice(), &deviceProperties);

			uint32_t maxPushConstantSize = deviceProperties.limits.maxPushConstantsSize;
			
			std::cout << "Max Push Constant Size: " << maxPushConstantSize << " bytes" << std::endl;
			
		lightsUBO = vRenderer->CreateUniformBuffers<LightsData>();
		cameraUBO = vRenderer->CreateUniformBuffers<CameraData>();
			
			bool everythingLoaded = true;
			for (const std::string& name : names) {
				auto iterator = assetManager->xmlAssets.find(name);
				// Is a name missing or has a typo?
				if (iterator == assetManager->xmlAssets.end()) {
					// Strings are handy to use the "+" symbol to join them up
					Debug::Error("Actor not found in Scene1.xml: " + name, __FILE__, __LINE__);
					everythingLoaded = false;
					continue; // skip to the next iteration
				}
				ActorList[name] = std::dynamic_pointer_cast<Actor>(iterator->second);
			}
			

			std::lock_guard<std::mutex> lock(vulkanMutex);
			for (const auto& pair : ActorList) {
			    Ref<Actor> actor = pair.second;
			    if (actor == nullptr ||
			       actor->GetComponent<DescriptorSetInfo>()   == nullptr ||
			       actor->GetComponent<Sampler2D>()			  == nullptr ||
			       actor->GetComponent<IndexedVertexBuffer>() == nullptr)
			    {
			       Debug::Error("Actor is missing a shader, material, mesh or shape: " + pair.first, __FILE__, __LINE__);
			       everythingLoaded = false;
			    }
			    else
			    {
			          *actor->GetComponent<Sampler2D>() =  vRenderer->Create2DTextureImage(actor->GetComponent<Sampler2D>()->filename.c_str());
			          *actor->GetComponent<IndexedVertexBuffer>() = vRenderer->LoadModelIndexed(actor->GetComponent<IndexedVertexBuffer>()->filename.c_str());
			          
			          std::string vertName = actor->GetComponent<DescriptorSetInfo>()->VertFilename;
			          std::string fragName = actor->GetComponent<DescriptorSetInfo>()->FragFilename;
			          {
			             DescriptorSetBuilder actorDescriptorBuilder(vRenderer->getDevice());
			             actorDescriptorBuilder.add(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1, actor->GetComponent<Sampler2D>().get());
			             *actor->GetComponent<DescriptorSetInfo>() = actorDescriptorBuilder.BuildDescriptorSet(vRenderer->getNumSwapchains());
			          }
			    	// create graphics pipeline for every actor, might be useful if two actors have different shader files. NOT MESHS OR TEXTURES LAYOUTS 
			    	// which is highly unlikely
			    	// which will also curse the camera setup so lets just not for now and just use one pipeline for everything
			       /*std::vector<VkDescriptorSetLayout> pipelineLayouts = {
			           CameraUBOinfo.descriptorSetLayout, 
			           actor->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			       };
			    
			       pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, vertName.c_str(), fragName.c_str());*/
			    }
			}
			// 0
			lights.diffuse[0] = Vec4(0.0, 0.0, 0.9, 0.0);
			lights.specular[0] = Vec4(0.0, 0.0, 0.3, 0.0);
			lights.ambient = Vec4(0.01, 0.01, 0.01, 0.0);
			lights.numLights = 4;
			lights.pos[0] = Vec4(-4.0f, -5.0f, -5.0f, 0.0f);
			// 1
			lights.diffuse[1] = Vec4(0.0, 0.9, 0.0, 0.0);
			lights.specular[1] = Vec4(0.0, 0.3, 0.0, 0.0);
			lights.pos[1] = Vec4(-4.0f, 5.0f, -5.0f, 0.0f);
			// 2
			lights.diffuse[2] = Vec4(0.9, 0.0, 0.0, 0.0);
			lights.specular[2] = Vec4(0.3, 0.0, 0.0, 0.0);
			lights.pos[2] = Vec4(4.0f, 5.0f, -5.0f, 0.0f);
			// 2
			lights.diffuse[3] = Vec4(0.5, 0.0, 0.5, 0.0);
			lights.specular[3] = Vec4(0.1, 0.0, 0.1, 0.0);
			lights.pos[3] = Vec4(4.0f, -5.0f, -5.0f, 0.0f);
			
			
			
			vRenderer->UpdateUniformBuffer<LightsData>(lights, lightsUBO);
			vRenderer->UpdateUniformBuffer<CameraData>(camera_actor_->GetCameraData(), cameraUBO);
			
			DescriptorSetBuilder descriptor_set_builder(vRenderer->getDevice());
			descriptor_set_builder.add(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 1, cameraUBO);
			descriptor_set_builder.add(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1, lightsUBO);
			CameraUBOinfo = descriptor_set_builder.BuildDescriptorSet(vRenderer->getNumSwapchains());
			
			// this makes one pipeline for everything 
			std::vector<VkDescriptorSetLayout> pipelineLayouts = {
				CameraUBOinfo.descriptorSetLayout, 
				ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
			};
			pipelineInfo = vRenderer->CreateGraphicsPipeline(pipelineLayouts, "shaders/multiPhong.vert.spv", "shaders/multiPhong.frag.spv");
			scene_number = 0;
		}
		break;

	case RendererType::OPENGL: 
		break;
	}
	MonitorTimingEnd("Scene1 OnCreate");
	return true;
}

void Scene1::HandleEvents(const SDL_Event& sdlEvent) {
	camera_actor_->SetQuat(sdlEvent);
		switch (sdlEvent.type) {
			
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			printf("size changed %d %d\n", sdlEvent.window.data1, sdlEvent.window.data2);
			float aspectRatio = static_cast<float>(sdlEvent.window.data1) / static_cast<float>(sdlEvent.window.data2);
			camera_actor_->UpdateProjectionMatrix(45.0f, aspectRatio, 0.5f, 100.0f);
			if (renderer->getRendererType() == RendererType::VULKAN)
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
				vRenderer->RecreateSwapChain();
				vRenderer->DestroyPipeline(pipelineInfo);
				std::vector<VkDescriptorSetLayout> pipelineLayouts = {
					CameraUBOinfo.descriptorSetLayout, 
					ActorList.begin()->second->GetComponent<DescriptorSetInfo>()->descriptorSetLayout
				};
				pipelineInfo = vRenderer->CreateGraphicsPipeline(
					pipelineLayouts, 
					"shaders/multiPhong.vert.spv", 
					"shaders/multiPhong.frag.spv"
				);
			}
			break;
		}
	
}
void Scene1::Update(const float deltaTime) {
	
	if (renderer->getRendererType() == RendererType::VULKAN)
	{
		VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
		camera_actor_->CameraMovement(deltaTime, nullptr);
		vRenderer->UpdateUniformBuffer<CameraData>(camera_actor_->GetCameraData(), cameraUBO);
		
		LightsData viewSpaceLights = lights; 
		for (int i = 0; i < lights.numLights; i++)
		{
			Vec4 worldPosition = lights.pos[i]; 
			viewSpaceLights.pos[i] = camera_actor_->GetViewMatrix() * worldPosition; 
		}
		vRenderer->UpdateUniformBuffer<LightsData>(viewSpaceLights, lightsUBO);
		
	}
	elapsedTime += deltaTime;
	ActorList.at("MarioFire")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(0.0f, 1.0f, 0.0f))) ;
	ActorList.at("MarioMime")->GetComponent<TransformComponent>()->SetOrientation
	(QMath::angleAxisRotation(elapsedTime * 90.0f, Vec3(1.0f, 0.0f, 0.0f))) ;
	
	if (elapsedTime >= 5.0f && !hasLoaded)
	{
		scene_number = 0;
		loadStagedScene = true;
		hasLoaded = true; // Lock it out from firing again
	}
}

void Scene1::Render() const {
		switch (renderer->getRendererType())
		{
		case RendererType::VULKAN:
			{
				VulkanRenderer* vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
				std::lock_guard<std::mutex> lock(vulkanMutex);
				vRenderer->RecordCommandBuffers(Recording::START);
				// Bind pipeline
				vRenderer->BindPipeline(pipelineInfo.pipeline);
				// bind Camera Once
				vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 0, CameraUBOinfo.descriptorSet);
				
				for (const auto& [name, actor] : ActorList) {
					switch (camera_actor_->CheckFrustum(actor->GetModelMatrix()) ) { 
					case FrustumCollision::CULLED_NEAR:
						std::cout << name << ": Clipped through Near Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_FAR:
						std::cout << name << ": Clipped through Far Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_RIGHT:
						std::cout << name << ": Clipped through Right Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_LEFT:
						std::cout << name << ": Clipped through Left Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_TOP:
						std::cout << name << ": Clipped through Top Plane" << std::endl;
						break;
					case FrustumCollision::CULLED_BOTTOM:
						std::cout << name << ": Clipped through Bottom Plane" << std::endl;
						break; 
					default:
						// bind mr mario
						vRenderer->BindDescriptorSet(pipelineInfo.pipelineLayout, 1, actor->GetComponent<DescriptorSetInfo>()->descriptorSet); 
						// set push constant
						vRenderer->SetPushConstant(pipelineInfo, actor->GetModelMatrix());
						// bind his mesh
						vRenderer->BindMesh(*actor->GetComponent<IndexedVertexBuffer>());
						// give him pants
						vRenderer->DrawIndexed(*actor->GetComponent<IndexedVertexBuffer>());
						break;
					}
				}
				// draw
				vRenderer->RecordCommandBuffers(Recording::STOP);
				vRenderer->Render();
				break;
			}

	case RendererType::OPENGL:
		OpenGLRenderer* glRenderer;
		glRenderer = dynamic_cast<OpenGLRenderer*>(renderer);
		/// Clear the screen
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		/// Draw your scene here
		glUseProgram(0);
		
		break;
	}
}


void Scene1::OnDestroy() {
VulkanRenderer* vRenderer;
    vRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    if(vRenderer){
       vkDeviceWaitIdle(vRenderer->getDevice());

       vRenderer->DestroyPipeline(pipelineInfo);
       vRenderer->DestroyUBO(lightsUBO);
       vRenderer->DestroyUBO(cameraUBO);
       vRenderer->DestroyDescriptorSet(CameraUBOinfo);
       
       for (const auto& [name, actor] : ActorList)
       {
          vRenderer->DestroyDescriptorSet(*actor->GetComponent<DescriptorSetInfo>());
          vRenderer->DestroySampler2D(*actor->GetComponent<Sampler2D>());
          vRenderer->DestroyIndexedMesh(*actor->GetComponent<IndexedVertexBuffer>());
       }
       ActorList.clear();
       assetManager.reset();
    }
}
