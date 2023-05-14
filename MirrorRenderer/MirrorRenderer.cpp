#include "MirrorRenderer.h"

#include "Utils/UI/TextRenderer.h"


namespace Falcor::Tutorial
{
    bool MirrorRenderer::RenderSettings::onGuiRender(Gui::Window& window)
    {
        bool hasSettingsChanged = false;

        static const Gui::DropdownList cullModeList = {
            {static_cast<uint32_t>(RasterizerState::CullMode::Front), "Front"},
            {static_cast<uint32_t>(RasterizerState::CullMode::Back), "Back"},
            {static_cast<uint32_t>(RasterizerState::CullMode::None), "None"}};

        if (window.dropdown("Cull mode", cullModeList, reinterpret_cast<uint32_t&>(cullMode)))
            hasSettingsChanged = true;

        static const Gui::DropdownList fillModeList = {
            {static_cast<uint32_t>(RasterizerState::FillMode::Solid), "Solid"},
            {static_cast<uint32_t>(RasterizerState::FillMode::Wireframe), "Wire-frame"}
        };

        if (window.dropdown("Fill mode", fillModeList, reinterpret_cast<uint32_t&>(fillMode)))
            hasSettingsChanged = true;

        window.checkbox("Show fps", showFPS);

        return hasSettingsChanged;
    }

    void MirrorRenderer::LightSettings::onGuiRender(Gui::Window& window)
    {
        if (auto lightGroup = window.group("Directional light settings"))
        {
            window.rgbColor("light ambient", ambient);
            window.rgbColor("light diffuse", diffuse);
            window.rgbColor("light specular", specular);
            window.var("light direction", lightDir);
        }
    }

    void MirrorRenderer::onLoad(RenderContext* pRenderContext)
    {
        mpDevice = getDevice();

        Program::Desc programDesc;
        programDesc.addShaderLibrary("Samples/MirrorRenderer/MirrorRenderer.vs.slang").vsEntry("main");
        programDesc.addShaderLibrary("Samples/MirrorRenderer/MirrorRenderer.ps.slang").psEntry("main");

        const GraphicsProgram::SharedPtr pProgram = GraphicsProgram::create(mpDevice, programDesc);
        mpGraphicsState = GraphicsState::create(mpDevice);
        mpGraphicsState->setProgram(pProgram);
        mpVars = GraphicsVars::create(mpDevice, pProgram->getReflector());

        applyRasterStateSettings();

        DepthStencilState::Desc dsDesc;
        dsDesc.setDepthEnabled(true);
        mpGraphicsState->setDepthStencilState(DepthStencilState::create(dsDesc));

        mpTextureSampler = Sampler::create(mpDevice.get(), {});

        mpMainCamera = Camera::create("main camera");
        mpMainCamera->setPosition({6, 3, 3});
        mpMainCamera->setTarget({0, 0, 0});
        mpMainCamera->setDepthRange(0.1f, 1000.f);
        mpMainCamera->setFocalLength(30.f);

        mpCameraController = FirstPersonCameraController::create(mpMainCamera);

        // TODO: remove
        mObjects.push_back(Object(TriangleMesh::createSphere(), mpDevice.get(), "sphere"));
        mObjects.push_back(Object(TriangleMesh::createSphere(), mpDevice.get(), "sphere2"));
    }

    void MirrorRenderer::onResize(uint32_t width, uint32_t height)
    {
        const float h = static_cast<float>(height);
        const float w = static_cast<float>(width);

        if (mpMainCamera != nullptr)
        {
            mpMainCamera->setFocalLength(30.f);
            const float aspectRatio = (w / h);
            mpMainCamera->setAspectRatio(aspectRatio);
        }
    }

    void MirrorRenderer::onFrameRender(RenderContext* pRenderContext, const std::shared_ptr<Fbo>& pTargetFbo)
    {
        mpCameraController->update();
        pRenderContext->clearFbo(pTargetFbo.get(), {0, 0.25, 0, 1}, 1.0f, 0, FboAttachmentType::All);

        mpGraphicsState->setFbo(pTargetFbo);

        mpVars["VSCBuffer"]["viewProjection"] = mpMainCamera->getViewProjMatrix();
        mpVars["PSCBuffer"]["cameraPosition"] = mpMainCamera->getPosition();

        for (const auto& object : mObjects)
        {
            mpVars["VSCBuffer"]["model"] = object.getTransform().getMatrix();

            mpVars["PSCBuffer"]["lightAmbient"] = mSettings.lightSettings.ambient;
            mpVars["PSCBuffer"]["lightDiffuse"] = mSettings.lightSettings.diffuse;
            mpVars["PSCBuffer"]["lightSpecular"] = mSettings.lightSettings.specular;
            mpVars["PSCBuffer"]["lightDir"] = mSettings.lightSettings.lightDir;
            mpVars["PSCBuffer"]["materialAmbient"] = object.getSettings().ambient;
            mpVars["PSCBuffer"]["materialDiffuse"] = object.getSettings().diffuse;
            mpVars["PSCBuffer"]["materialSpecular"] = object.getSettings().specular;

            const bool hasTexture = object.getTexture() != nullptr;
            mpVars["PSCBuffer"]["isTextureLoaded"] = hasTexture;
            if (hasTexture)
            {
                mpVars["PSCBuffer"]["objTexture"] = object.getTexture();
                mpVars["PSCBuffer"]["texSampler"] = mpTextureSampler;
            }

            mpGraphicsState->setVao(object.getVao());
            pRenderContext->drawIndexed(mpGraphicsState.get(), mpVars.get(), object.getIndexCount(), 0, 0);
        }

        // TODO: render mirror(s)

        mFrameRate.newFrame();
        if (mSettings.renderSettings.showFPS)
            TextRenderer::render(pRenderContext, mFrameRate.getMsg(), pTargetFbo, {10, 10});
    }

    void MirrorRenderer::onGuiRender(Gui* pGui)
    {
        Gui::Window window(pGui, "Settings", {375, 275}, {0, 30});

        if (mSettings.renderSettings.onGuiRender(window))
            applyRasterStateSettings();

        mSettings.lightSettings.onGuiRender(window);

        for (auto& object : mObjects)
        {
            object.onGuiRender(window);
        }
    }

    bool MirrorRenderer::onKeyEvent(const KeyboardEvent& keyEvent)
    {
        return mpCameraController->onKeyEvent(keyEvent);
    }

    bool MirrorRenderer::onMouseEvent(const MouseEvent& mouseEvent)
    {
        return mpCameraController->onMouseEvent(mouseEvent);
    }

    void MirrorRenderer::createMirror(const float3& pos)
    {
        // TODO
    }

    void MirrorRenderer::applyRasterStateSettings() const
    {
        if (mpGraphicsState == nullptr)
            return;

        RasterizerState::Desc rsDesc;
        rsDesc.setCullMode(mSettings.renderSettings.cullMode);
        rsDesc.setFillMode(mSettings.renderSettings.fillMode);
        mpGraphicsState->setRasterizerState(RasterizerState::create(rsDesc));
    }
}

int main()
{
    Falcor::SampleAppConfig config;
    config.windowDesc.width = 1280;
    config.windowDesc.height = 720;
    config.windowDesc.title = "Mirror renderer";

    Falcor::Tutorial::MirrorRenderer mirrorRenderer(config);
    return mirrorRenderer.run();
}