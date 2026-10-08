#include <Core/Actor.h>
#include <Core/Debug.h>
#include <physics/TransformComponent.h>

Actor::Actor(std::weak_ptr<Component> parent_):Component(parent_) {}

bool Actor::OnCreate() {
    if (isCreated) return true;
    Debug::Info("Loading assets for Actor: ", __FILE__, __LINE__);
    
    // Since components are now in componentMap, we loop over the map table instead of the vector
    for (size_t i = 0; i < componentMap.getCapacity(); ++i) {

    }
    
    isCreated = true;
    return isCreated;
}

Actor::~Actor() {
    OnDestroy();
}

void Actor::OnDestroy() {
    Debug::Info("Deleting assets for Actor: ", __FILE__, __LINE__);
    RemoveAllComponents();
    isCreated = false;
}

void Actor::Update(const float deltaTime) {
    std::cout << "Hello from Update\n";
}

void Actor::Render() const {}


void Actor::ListComponents() const {
    std::cout << typeid(*this).name() << " contains the following components:\n";

    std::cout << '\n';
}

MATH::Matrix4 Actor::GetModelMatrix() {
    MATH::Matrix4 modelMatrix;
    Ref<TransformComponent> transform = GetComponent<TransformComponent>();

    if (transform) {
       modelMatrix = transform->GetTransformMatrix();
    }
    else {
       modelMatrix.loadIdentity();
    }

    if (auto lockedParent = parent.lock()) {
       // Fixed the dynamic cast for the raw pointer from the weak_ptr
       Actor* parentActor = dynamic_cast<Actor*>(lockedParent.get());
       if (parentActor) {
          auto parentTransform = parentActor->GetComponent<TransformComponent>();
          if (parentTransform) {
              // Fixed the matrix multiplication operator syntax
              modelMatrix = parentTransform->GetTranslate_Rotate_Matrix() * modelMatrix;
          }
       }
    }
    return modelMatrix;
}