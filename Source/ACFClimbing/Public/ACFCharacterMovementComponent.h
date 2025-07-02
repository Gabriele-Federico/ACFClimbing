#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ACFCharacterMovementComponent.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ACFCLIMBING_API UACFCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UACFCharacterMovementComponent();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(Server, Reliable)
	void TryClimbing();

	UFUNCTION(Server, Reliable)
	void CancelClimbing();

	UFUNCTION(BlueprintPure)
	bool IsClimbing() const;

	UFUNCTION(BlueprintPure)
	FVector GetClimbSurfaceNormal() const;

	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

private:

	void SweepAndStoreWallHits();

	bool IsWallClimbable(const FHitResult& Hit, const FVector& Forward) const noexcept;

	bool EyeHeightTrace(float TraceDistance) const noexcept;

	bool IsFacingSurface(float Steepness) const;

	void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	void PhysCustom(float DeltaTime, int32 Iterations) override;

	UFUNCTION(Server, Reliable)
	void PhysClimbing(float DeltaTime, int32 Iterations);

	void ComputeSurfaceInfo();
	
	void ComputeClimbingVelocity(float DeltaTime);
	
	bool ShouldStopClimbing();
	
	void StopClimbing(float DeltaTime, int32 Iterations);
	
	void MoveAlongClimbingSurface(float DeltaTime);
	
	void SnapToClimbingSurface(float DeltaTime) const;

	float GetMaxSpeed() const override;
	
	float GetMaxAcceleration() const override;

	FQuat GetClimbingRotation(float DeltaTime) const;

	bool ClimbDownToFloor() const;

	bool TryClimbUpLedge() const;

	bool HasReachedEdge() const;

	bool CanMoveToLedgeClimbLocation() const;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int32 CollisionCapsuleRadius = 50;
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int32 CollisionCapsuleHalfHeight = 72;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta=(ClampMin="1.0", ClampMax="75.0"))
	float MinHorizontalDegreesToStartClimbing = 25;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float ClimbingCollisionShrinkAmount = 30;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float MaxClimbingSpeed = 120.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "10.0", ClampMax = "2000.0"))
	float MaxClimbingAcceleration = 380.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "3000.0"))
	float BrakingDecelerationClimbing = 550.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "12.0"))
	int32 ClimbingRotationSpeed = 6;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "60.0"))
	float ClimbingSnapSpeed = 4.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float DistanceFromSurface = 45.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "1.0", ClampMax = "500.0"))
	float FloorCheckDistance = 100.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float ClimbUpVerticalOffset  = 160.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float ClimbUpHorizontalOffset = 80.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditDefaultsOnly)
	TObjectPtr<UAnimMontage> LedgeClimbMontage;

	UPROPERTY()
	TObjectPtr<UAnimInstance> AnimInstance;

	TArray<FHitResult> CurrentWallHits;
	FCollisionQueryParams ClimbQueryParams;

	UPROPERTY(replicated)
	FVector CurrentClimbingNormal;
	FVector CurrentClimbingPosition;

	bool bWantsToClimb;

};
