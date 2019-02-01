#include "RootManager.h"

void RootManager::EngineInitialization(Window& window, Camera& cam, Material& mat1, Material& mat2, DirectionalLight& light,
	Skybox& skybox, std::vector<Shader>& shaderList)
{
	CompileShaders();

	renderWindow = window;

	camera = Camera(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), -60.0f, 0.0f, 5.0f, 0.5f);

	mat1 = Material(1.0f, 4);
	defaultMaterial = mat1;
	mat2 = Material(0.3f, 4);

	light = DirectionalLight(2048, 2048,
		1.0f, 0.9568627f, 0.8392157f,
		0.026f, 0.9f,
		-10.0f, -12.0f, 18.5f);

	mainLight = light;

	std::vector<std::string> skyboxFaces;
	skyboxFaces.push_back("Textures/Skybox/lagoon_rt.tga");
	skyboxFaces.push_back("Textures/Skybox/lagoon_lf.tga");
	skyboxFaces.push_back("Textures/Skybox/lagoon_up.tga");
	skyboxFaces.push_back("Textures/Skybox/lagoon_dn.tga");
	skyboxFaces.push_back("Textures/Skybox/lagoon_bk.tga");
	skyboxFaces.push_back("Textures/Skybox/lagoon_ft.tga");
	skybox = Skybox(skyboxFaces);
	this->skybox = skybox;
}

void RootManager::EngineUpdate(Camera& cam, DirectionalLight* mainLight)
{
	//glCullFace(GL_FRONT);
	DirectionalShadowMapPass(mainLight);
	//glCullFace(GL_BACK);

	for (int i = 0; i < pointCount; i++)
	{
		OmniShadowMapPass(&pointLights[i]);
	}

	for (int i = 0; i < spotsCount; i++)
	{
		OmniShadowMapPass(&spotLights[i]);
	}

	RenderPass(cam.CalculateViewMatrix(), projection, pointLights, spotLights, pointCount, spotsCount);
	RenderScene();
}

void RootManager::EngineLoading()
{
	for (int i = 0; i < this->textureList->size(); i++)
	{
		NE_ERROR_CHECK(this->textureList->at(i)->LoadTexture());
	}

	for (int i = 0; i < this->objectList->size(); i++)
	{
		NE_ERROR_CHECK(this->objectList->at(i)->LoadModel());
	}

	this->loading = false;
}

void RootManager::EngineVariablesUpdate(std::vector<Texture*>* textureList, std::vector<GameObject*>* objectList,
	glm::mat4 projection, PointLight* points, SpotLight* spots, unsigned int pointsCount,
	unsigned int spotsCount)
{
	this->textureList = textureList;
	this->pointCount = pointsCount;
	this->objectList = objectList;
	this->projection = projection;
	this->spotsCount = spotsCount;
	this->points = points;
	this->spots = spots;
}

bool RootManager::MainLoop(bool windowShouldClose, Window& window, void* Start, void* Update)
{
	if (firstStart)
	{
		debugWindow = Ui(&renderWindow);
		// Manager for the entity-component-system
		manager = new Manager();

		{
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO(); (void)io;

			ImGui_ImplGlfw_InitForOpenGL(renderWindow.GetWindow(), true);
			ImGui_ImplOpenGL3_Init("#version 130");

			ImGui::StyleColorsDark();

			bool show_demo_window = true;
			bool show_another_window = true;
			ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
		}

		// Create nebula engine logo for the loading screen
		CreateNebulaLogo();
		// Load texture
		//NE_ERROR_CHECK(nebulaLogo.LoadTexture());

		((void(*)(void))Start)();

		firstStart = false;
	}
	else
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Loading
		if (loading)
		{
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			// ToDo
			nebulaLogoShader.UseShader();

			glm::mat4 model = glm::mat4();

			model = glm::translate(model, glm::vec3(-2.0f, 0.0f, -2.0f));

			nebulaLogoShader.SetMatrix("projection", projection);
			nebulaLogoShader.SetMatrix("view", camera.CalculateViewMatrix());
			nebulaLogoShader.SetMatrix("model", model);

			nebulaLogoShader.SetTexture("dTexture", 1);

			nebulaLogo.UseTexture(GL_TEXTURE1);
			nebulaEngineLogo.RenderMesh();

			window.SwapBuffers();
		}

		window.WindowUpdate();
		window.SetVSync(false);

		if (!loading)
		{
			camera.KeyControl(window.GetWindow(), window.GetDeltaTime());
			camera.MouseControl(window.GetXChange(), window.GetYChange());

			debugWindow.DebugWindow(true, renderWindow.GetFPS(), renderWindow.GetDeltaTime());

			// Update
			manager->Update();
			((void(*)(void))Update)();
		}
		else
		{
			EngineLoading();
		}

		EngineUpdate(camera, &mainLight);

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.SwapBuffers();
	}

	return false;
}

void RootManager::CompileCustomShaders(void * custom)
{
	((void(*)(void))custom)();
}

void RootManager::ShutDown()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void RootManager::RenderScene()
{
	glm::mat4 model;

	for (int i = 0; i < objectList->size(); i++)
	{
		model = glm::mat4();
		if (objectList->at(i)->GetUseBlending())
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
		}
		else
		{
			glDisable(GL_BLEND);
		}
		model = glm::translate(model, objectList->at(i)->GetPosition());
		model = glm::rotate(model, Math::ToRadians(objectList->at(i)->GetDegrees()), objectList->at(i)->GetRotation());
		model = glm::scale(model, objectList->at(i)->GetScale());
		shaderList[0].SetBool("renderNormalMaps", objectList->at(i)->GetRenderNormalMaps());
		shaderList[0].SetMatrix("model", model);
		shaderList[0].SetVector3("primaryColor", objectList->at(i)->GetMainColor());
		shaderList[0].SetBool("useNormalMap", objectList->at(i)->GetUseNormalMaps());
		if (uniformModel != 0)
			glUniformMatrix4fv(uniformModel, 1, false, glm::value_ptr(model));
		defaultMaterial.UseMaterial(&shaderList[0]);
		objectList->at(i)->RenderModel();
	}

	uniformModel = 0;
}

void RootManager::CreateNebulaLogo()
{
	float vertices[] =
	{
		0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
	};

	unsigned int indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};

	Util::CalculateNormals(indices, 6, vertices, 56, 14, 5);

	nebulaEngineLogo.CreateMesh(vertices, indices, 56, 6);
}

void RootManager::CompileShaders()
{
	Shader* shader1 = new Shader();
	NE_ERROR_CHECK(shader1->CreateFromFiles("Shaders/shader.vert", "Shaders/shader.frag"));
	shaderList.push_back(*shader1);
	
	NE_ERROR_CHECK(directionalShadowShader.CreateFromFiles("Shaders/directional_shadow_map.vert", "Shaders/directional_shadow_map.frag"));

	NE_ERROR_CHECK(omniShadowShader.CreateFromFiles("Shaders/omni_shadow_map.vert", "Shaders/omni_shadow_map.geom", "Shaders/omni_shadow_map.frag"));
	
	NE_ERROR_CHECK(nebulaLogoShader.CreateFromFiles("Shaders/logo.vert", "Shaders/logo.frag"));
}

void RootManager::OmniShadowMapPass(PointLight * light)
{
	omniShadowShader.UseShader();

	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight());

	light->GetShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	// uniformModel need to be applied! 
	uniformModel = omniShadowShader.GetModelLocation();

	omniShadowShader.SetVector3("lightPos", light->GetPosition());
	omniShadowShader.SetFloat("farPlane", light->GetFarPlane());
	omniShadowShader.SetLightMatrices(light->CalculateLightTransform());

	NE_ERROR_CHECK(omniShadowShader.Validate());

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RootManager::DirectionalShadowMapPass(DirectionalLight * light)
{
	directionalShadowShader.UseShader();

	glViewport(0, 0, light->GetShadowMap()->GetShadowWidth(), light->GetShadowMap()->GetShadowHeight());

	light->GetShadowMap()->Write();
	glClear(GL_DEPTH_BUFFER_BIT);

	// uniformModel need to be applied!
	uniformModel = directionalShadowShader.GetModelLocation();
	directionalShadowShader.SetDirectionalLightTransform(&light->CalculateLightTransform());

	directionalShadowShader.Validate();

	RenderScene();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RootManager::RenderPass(glm::mat4 viewMatrix, glm::mat4 projectionMatrix, PointLight* pointLights, 
	SpotLight* spotLights, int pointlightCount, int spotlightCount)
{
	glViewport(0, 0, renderWindow.GetWindowWidth(), renderWindow.GetWindowHeight());

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	skybox.DrawSkybox(viewMatrix, projectionMatrix);

	shaderList[0].UseShader();

	// Upload matrices
	shaderList[0].SetMatrix("projection", projectionMatrix);
	shaderList[0].SetMatrix("view", viewMatrix);
	shaderList[0].SetVector3("eyePosition", camera.GetCameraPosition());

	shaderList[0].SetDirectionalLight(&mainLight);
	shaderList[0].SetPointLights(pointLights, pointlightCount, 4, 0);
	shaderList[0].SetSpotLights(spotLights, spotlightCount, 4 + pointlightCount, pointlightCount);
	shaderList[0].SetDirectionalLightTransform(&mainLight.CalculateLightTransform());

	mainLight.GetShadowMap()->Read(GL_TEXTURE3);
	// Diffuse Texture
	shaderList[0].SetTexture("dTexture", 1);
	// Normal Texture
	shaderList[0].SetTexture("nTexture", 2);
	shaderList[0].SetDirectionalShadowMap(3);

	NE_ERROR_CHECK(shaderList[0].Validate());
}
