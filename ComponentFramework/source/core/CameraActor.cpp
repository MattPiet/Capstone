#include "core/CameraActor.h"
#include <MMath.h>
#include <SDL3/SDL.h>
#include <QMath.h>
#include "Core/Debug.h"
#include <assert.h>
#include "Physics/TransformComponent.h"
using namespace MATH;
CameraActor::CameraActor(std::weak_ptr<Component> parent_, float fovy, float aspectRatio, float near, float far) : Actor(parent_)
{
	projectionMatrix = MMath::PerspectiveVulkan(fovy, aspectRatio, near, far);
	viewMatrix.loadIdentity();
	UpdateFrustum();
}

CameraActor::~CameraActor() {
	OnDestroy();
}

bool CameraActor::OnCreate()
{
	// This'll stop us from creating something twice and generating memory leaks
	// This will save us sweating when the code gets even more complicated
	if (isCreated) return true;

	isCreated = true;
	return isCreated;
}

void CameraActor::UpdateProjectionMatrix(const float fovy, const float aspectRatio, const float near, const float far)
{
	projectionMatrix = MMath::PerspectiveVulkan(fovy, aspectRatio, near, far);
	UpdateFrustum();

}

void CameraActor::UpdateViewMatrix()
{
	Ref<TransformComponent> transformComponent = GetComponent<TransformComponent>();
	if (transformComponent == nullptr) {
		viewMatrix = MMath::lookAt(Vec3(0.0f, 0.0f, 5.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));
	}
	else {
		Quaternion orientation = transformComponent->GetQuaternion();
		Vec3 position = transformComponent->GetPosition();
		position_orientation_Quat = MATHEX::DQMath::rotate(QMath::conjugate(orientation)) * MATHEX::DQMath::translate(-position);
		UpdateFrustum();
	}
}

void CameraActor::OnDestroy()
{
	isCreated = false;
}

Vec3 CameraActor::freeCameraMovement(Vec3 direction)
{
	Matrix4 worldToCamera = MMath::toMatrix4(position_orientation_Quat);
	Matrix4 cameraToWorld = MMath::inverse(worldToCamera);
	Vec3 rotated_forward_in_cam_space = cameraToWorld * direction;
	return rotated_forward_in_cam_space;
}

void CameraActor::CameraMovement(float deltaTime, SDL_Gamepad* gamepad)
{

    Vec3 inputVelocity(0.0f, 0.0f, 0.0f);
    const float deadzone = 0.2f; 
    const int16_t triggerDeadzone = 8000; 

    if (gamepad && SDL_GamepadConnected(gamepad)) {
       float stickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
       float stickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

       if (SDL_fabsf(stickX) > deadzone) inputVelocity.x += stickX * 2;
       if (SDL_fabsf(stickY) > deadzone) inputVelocity.z += stickY * 2;

       int16_t leftTrigger  = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
       int16_t rightTrigger = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);

       if (leftTrigger > triggerDeadzone)  inputVelocity.y -= 1.0f;
       if (rightTrigger > triggerDeadzone) inputVelocity.y += 1.0f;
    }
	
    const bool* keyboardState = SDL_GetKeyboardState(NULL);
    if (keyboardState[SDL_SCANCODE_W])      inputVelocity.z -= 1.0f;
    if (keyboardState[SDL_SCANCODE_S])      inputVelocity.z += 1.0f;
    if (keyboardState[SDL_SCANCODE_A])      inputVelocity.x -= 1.0f;
    if (keyboardState[SDL_SCANCODE_D])      inputVelocity.x += 1.0f;
    if (keyboardState[SDL_SCANCODE_SPACE])  inputVelocity.y += 1.0f;
    if (keyboardState[SDL_SCANCODE_LSHIFT]) inputVelocity.y -= 1.0f;

    Ref<TransformComponent> transform = GetComponent<TransformComponent>();
    Quaternion currentOrientation = transform->GetQuaternion();

    if (gamepad && SDL_GamepadConnected(gamepad)) {
       float rightStickX = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f;
       float rightStickY = SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f;

       if (SDL_fabsf(rightStickX) > deadzone || SDL_fabsf(rightStickY) > deadzone) {
           m_Yaw += -rightStickX * c_Sensitivity * deltaTime;
           m_Pitch += -rightStickY * c_Sensitivity * deltaTime;

          if (m_Pitch > 89.0f)  m_Pitch = 89.0f;
          if (m_Pitch < -89.0f) m_Pitch = -89.0f;

          Quaternion qYaw = QMath::angleAxisRotation(m_Yaw, Vec3(0.0f, 1.0f, 0.0f));
          Quaternion qPitch = QMath::angleAxisRotation(m_Pitch, Vec3(1.0f, 0.0f, 0.0f));
          currentOrientation = qYaw * qPitch;
       }
    }
	
    if (VMath::mag(inputVelocity) > 0.0f) {
       if (VMath::mag(inputVelocity) > 1.0f) {
           inputVelocity = VMath::normalize(inputVelocity);
       }
       
       Vec3 displacement = inputVelocity * GetCameraSpeed() * deltaTime;
       SetView(currentOrientation, freeCameraMovement(displacement));
    } else {
       SetView(currentOrientation, transform->GetPosition());
    }
	
    UpdateViewMatrix();
}
void CameraActor::SetView(const Quaternion& orientation_, const Vec3& position_) {
	GetComponent<TransformComponent>()->SetOrientation(orientation_ );
	GetComponent<TransformComponent>()->SetPosition(position_);
}

void CameraActor::SetQuat(const SDL_Event& sdlEvent)
{
	if (sdlEvent.type == SDL_EVENT_MOUSE_WHEEL)
	{
		if (SDL_GetGlobalMouseState(NULL, NULL) & SDL_BUTTON_RMASK) {
			CameraSpeed += sdlEvent.wheel.y * 2.0f; 
		}
		if (CameraSpeed < 0.0f) CameraSpeed = 0.0f;
		if (CameraSpeed > 100.0f) CameraSpeed = 100.0f;
	}
	// Only let the trackball update if it's a mouse event
	if (sdlEvent.type == SDL_EVENT_MOUSE_MOTION ||
		sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
		sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		trackball.HandleEvents(sdlEvent);
		GetComponent<TransformComponent>()->SetOrientation(trackball.getQuat()); // Sync camera to trackball
		}
	UpdateViewMatrix();
}

//![Frustrum](../../ExamplePhotos/Frustrum.png)
void CameraActor::UpdateFrustum()
{
	/*
	 *
	 Matrix4x4 ([ 0 , 4 , 8 , 12 
				  1 , 5 , 9 , 12
				  2 , 6 , 10 , 14
				  3 , 7 , 11 , 15 ] );
	
	Row 1 (computes X): [ vp[0], vp[4], vp[8], vp[12] ]

	Row 2 (computes Y): [ vp[1], vp[5], vp[9], vp[13] ]

	Row 3 (computes Z): [ vp[2], vp[6], vp[10], vp[14] ]

	Row 4 (computes W): [ vp[3], vp[7], vp[11], vp[15] ]
	  
	 */
	
	//  Calculate the combined View-Projection matrix
	Matrix4 vp = projectionMatrix * GetViewMatrix();
	
	// Left plane: row 4 + row 1
	frustum.left_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] + vp[0], vp[7] + vp[4], vp[11] + vp[8], vp[15] + vp[12]
	));

	// Right plane: row 4 - row 1
	frustum.right_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] - vp[0], vp[7] - vp[4], vp[11] - vp[8], vp[15] - vp[12]
	));

	// Bottom plane: row 4 + row 2
	frustum.bottom_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] + vp[1], vp[7] + vp[5], vp[11] + vp[9], vp[15] + vp[13]
	));

	// Top plane: row 4 - row 2
	frustum.top_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] - vp[1], vp[7] - vp[5], vp[11] - vp[9], vp[15] - vp[13]
	));

	// Near plane: row 4 + row 3
	frustum.front_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] + vp[2], vp[7] + vp[6], vp[11] + vp[10], vp[15] + vp[14]
	));

	// Far plane: row 4 - row 3
	frustum.back_plane = MATHEX::PMath::normalize(MATHEX::Plane(
		vp[3] - vp[2], vp[7] - vp[6], vp[11] - vp[10], vp[15] - vp[14]
	));
}

FrustumCollision CameraActor::CheckFrustum(Matrix4 modelMatrix)
{
	Vec3 position(modelMatrix[12], modelMatrix[13], modelMatrix[14]);
	
	if (MATHEX::PMath::distance(position, frustum.front_plane) < -cullRadius) {
		return FrustumCollision::CULLED_NEAR; 
	}
	if (MATHEX::PMath::distance(position, frustum.left_plane) < -cullRadius) {
		return FrustumCollision::CULLED_LEFT; 
	}
	if (MATHEX::PMath::distance(position, frustum.right_plane) < -cullRadius) {
		return FrustumCollision::CULLED_RIGHT; 
	}
	if (MATHEX::PMath::distance(position, frustum.back_plane) < -cullRadius) {
		return FrustumCollision::CULLED_FAR; 
	}
	if (MATHEX::PMath::distance(position, frustum.bottom_plane) < -cullRadius) {
		return FrustumCollision::CULLED_BOTTOM; 
	}
	if (MATHEX::PMath::distance(position, frustum.top_plane) < -cullRadius) {
		return FrustumCollision::CULLED_TOP;
	}
	return FrustumCollision::INSIDE;
}



