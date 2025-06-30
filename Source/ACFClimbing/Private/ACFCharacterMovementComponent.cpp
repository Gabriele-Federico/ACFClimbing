#include "ACFCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "ACFCustomMovementModes.h"

UACFCharacterMovementComponent::UACFCharacterMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UACFCharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ClimbQueryParams.AddIgnoredActor(GetOwner());
}

void UACFCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SweepAndStoreWallHits();
}

void UACFCharacterMovementComponent::TryClimbing() 
{
	if (CanStartClimbing()) 
	{
		bWantsToClimb = true;
	}
}

void UACFCharacterMovementComponent::CancelClimbing() 
{
	bWantsToClimb = false;
}

bool UACFCharacterMovementComponent::IsClimbing() const {
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == EACFCustomMovementMode::Climbing;
}

FVector UACFCharacterMovementComponent::GetClimbSurfaceNormal() const {
	return CurrentClimbingNormal;
}

void UACFCharacterMovementComponent::SweepAndStoreWallHits() 
{
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);
	
	const FVector StartOffset = UpdatedComponent->GetForwardVector() * 20;

	// Avoid using the same Start/End location for a Sweep, as it doesn't trigger hits on Landscapes.
	const FVector Start = UpdatedComponent->GetComponentLocation() + StartOffset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();

	TArray<FHitResult> Hits;
	const bool HitWall = GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_WorldStatic, CollisionShape, ClimbQueryParams);
	
	#ifdef WITH_EDITOR
	DrawDebugCapsule(GetWorld(), Start, CollisionCapsuleHalfHeight, CollisionCapsuleRadius, FQuat::Identity, FColor::Green, false, -1, 0, 3);

	for (const auto& Hit : Hits) 
	{
		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 5.f, 8, FColor::Yellow, false, -1.f, 0, .5f);
	}
	#endif
	
	if (HitWall) {
		CurrentWallHits = Hits;
	}
	else {
		CurrentWallHits.Reset();
	}
}

bool UACFCharacterMovementComponent::CanStartClimbing() const noexcept
{
	for (const FHitResult& Hit : CurrentWallHits) 
	{
		const FVector HorizontalNormal = Hit.Normal.GetSafeNormal2D();

		const float HorizontalDot = FVector::DotProduct(UpdatedComponent->GetForwardVector(), -HorizontalNormal);
		const float VerticalDot = FVector::DotProduct(Hit.Normal, HorizontalNormal);

		const float HorizontalDegrees = FMath::RadiansToDegrees(FMath::Acos(HorizontalDot));

		const bool bIsCeiling = FMath::IsNearlyZero(VerticalDot);

		if (HorizontalDegrees <= MinHorizontalDegreesToStartClimbing && !bIsCeiling && IsFacingSurface(VerticalDot))
		{
			return true;
		}
	}
	return false;
}

bool UACFCharacterMovementComponent::EyeHeightTrace(const float TraceDistance) const noexcept
{
	FHitResult UpperEdgeHit;

	const FVector Start = UpdatedComponent->GetComponentLocation() + (UpdatedComponent->GetUpVector() * GetCharacterOwner()->BaseEyeHeight);
	const FVector End = Start + (UpdatedComponent->GetForwardVector() * TraceDistance);
	#if WITH_EDITOR
	DrawDebugLine(GetWorld(), Start, End, FColor::Red, false, -1.f, 0, 1.f);
	#endif
	return GetWorld()->LineTraceSingleByChannel(UpperEdgeHit, Start, End, ECC_WorldStatic, ClimbQueryParams);
}

bool UACFCharacterMovementComponent::IsFacingSurface(const float Steepness) const 
{
	constexpr float BASE_LENGTH = 80.f;
	const float SteepnessMultiplier = 1 + (1 - Steepness) * 5;

	return EyeHeightTrace(BASE_LENGTH * SteepnessMultiplier);
}

void UACFCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	if (bWantsToClimb) 
	{
		SetMovementMode(EMovementMode::MOVE_Custom, EACFCustomMovementMode::Climbing);
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UACFCharacterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) 
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		
		// TODO: Check if needed
		//UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		//Capsule->SetCapsuleHalfHeight(Capsule->GetUnscaledCapsuleHalfHeight() - ClimbingCollisionShrinkAmount);
	}

	if (PreviousMovementMode == EMovementMode::MOVE_Custom && PreviousCustomMode == EACFCustomMovementMode::Climbing)
	{
		bOrientRotationToMovement = true;
		const FRotator StandRotation = FRotator(0., UpdatedComponent->GetComponentRotation().Yaw, 0.);
		UpdatedComponent->SetRelativeRotation(StandRotation);
		
		// TODO: Check if needed
		//UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
		//Capsule->SetCapsuleHalfHeight(Capsule->GetUnscaledCapsuleHalfHeight() + ClimbingCollisionShrinkAmount);
		
		StopMovementImmediately();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UACFCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations) 
{
	if (CustomMovementMode == EACFCustomMovementMode::Climbing) 
	{
		PhysClimbing(DeltaTime, Iterations);
	}

	Super::PhysCustom(DeltaTime, Iterations);
}

void UACFCharacterMovementComponent::PhysClimbing(float DeltaTime, int32 Iterations) 
{
	if(DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	ComputeSurfaceInfo();

	if (ShouldStopClimbing() || ClimbDownToFloor())
	{
		StopClimbing(DeltaTime, Iterations);
		return;
	}

	ComputeClimbingVelocity(DeltaTime);

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();

	MoveAlongClimbingSurface(DeltaTime);

	TryClimbUpLedge();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}

	SnapToClimbingSurface(DeltaTime);

}

void UACFCharacterMovementComponent::ComputeSurfaceInfo() 
{
	CurrentClimbingNormal = FVector::ZeroVector;
	CurrentClimbingPosition = FVector::ZeroVector;

	if (CurrentWallHits.IsEmpty()) 
	{
		return;
	}

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FCollisionShape CollisionSphere = FCollisionShape::MakeSphere(6);

	for (const auto& Hit : CurrentWallHits) 
	{
		const FVector End = Start + (Hit.ImpactPoint - Start).GetSafeNormal() * 120;

		FHitResult AssistHit;
		GetWorld()->SweepSingleByChannel(AssistHit, Start, End, FQuat::Identity, ECC_WorldStatic, CollisionSphere, ClimbQueryParams);

		CurrentClimbingPosition += AssistHit.ImpactPoint;
		CurrentClimbingNormal += AssistHit.Normal;
	}

	CurrentClimbingPosition /= CurrentWallHits.Num();
	CurrentClimbingNormal = CurrentClimbingNormal.GetSafeNormal();

	#if WITH_EDITOR
	DrawDebugSphere(GetWorld(), CurrentClimbingPosition, 5.f, 8, FColor::Blue, false, -1.f, 0, .5f);
	DrawDebugLine(GetWorld(), CurrentClimbingPosition, CurrentClimbingPosition + 10.f * CurrentClimbingNormal, FColor::Blue, false, -1.f, 0, 1.f);
	#endif

}

void UACFCharacterMovementComponent::ComputeClimbingVelocity(float DeltaTime) 
{
	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity()) 
	{
		constexpr float FRICTION = .0f;
		constexpr bool bFLUID = false;
		CalcVelocity(DeltaTime, FRICTION, bFLUID, BrakingDecelerationClimbing);
	}

	ApplyRootMotionToVelocity(DeltaTime);
}

bool UACFCharacterMovementComponent::ShouldStopClimbing() 
{
	const bool bIsOnCeiling = FVector::Parallel(CurrentClimbingNormal, FVector::UpVector);
	return !bWantsToClimb || CurrentClimbingNormal.IsZero() || bIsOnCeiling;
}

void UACFCharacterMovementComponent::StopClimbing(float DeltaTime, int32 Iterations) 
{
	bWantsToClimb = false;
	SetMovementMode(EMovementMode::MOVE_Falling);
	StartNewPhysics(DeltaTime, Iterations);
}

void UACFCharacterMovementComponent::MoveAlongClimbingSurface(float DeltaTime) 
{
	const FVector Adjusted = Velocity * DeltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, GetClimbingRotation(DeltaTime), true, Hit);

	if (Hit.Time < 1.f) 
	{
		HandleImpact(Hit, DeltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}
}

void UACFCharacterMovementComponent::SnapToClimbingSurface(float DeltaTime) const 
{
	const FVector Forward = UpdatedComponent->GetForwardVector();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();
	
	const FVector ForwardDifference = (CurrentClimbingPosition - Location).ProjectOnTo(Forward);
	const FVector Offset = -CurrentClimbingNormal * (ForwardDifference.Length() - DistanceFromSurface);

	constexpr bool bSWEEP = true;
	UpdatedComponent->MoveComponent(Offset * ClimbingSnapSpeed * DeltaTime, Rotation, bSWEEP);
}

float UACFCharacterMovementComponent::GetMaxSpeed() const 
{
	return IsClimbing() ? MaxClimbingSpeed : Super::GetMaxSpeed();
}

float UACFCharacterMovementComponent::GetMaxAcceleration() const 
{
	return IsClimbing() ? MaxClimbingAcceleration : Super::GetMaxAcceleration();
}

FQuat UACFCharacterMovementComponent::GetClimbingRotation(float DeltaTime) const 
{
	const FQuat Current = UpdatedComponent->GetComponentQuat();
	const FQuat Target = FRotationMatrix::MakeFromX(-CurrentClimbingNormal).ToQuat();
	return FMath::QInterpTo(Current, Target, DeltaTime, ClimbingRotationSpeed);
}

bool UACFCharacterMovementComponent::ClimbDownToFloor() const 
{
	FHitResult FloorHit = CheckFloor();
	if (!FloorHit.bBlockingHit) 
	{
		return false;
	}

	const bool bOnWalkableFloor = FloorHit.Normal.Z > GetWalkableFloorZ();
	const float DownSpeed = FVector::DotProduct(Velocity, -FloorHit.Normal);
	const bool bIsMovingTowardsFloor = DownSpeed >= MaxClimbingSpeed / 3 && bOnWalkableFloor;

	const bool bIsClimbingFloor = CurrentClimbingNormal.Z > GetWalkableFloorZ();

	return bIsMovingTowardsFloor || (bIsClimbingFloor && bOnWalkableFloor);
}

FHitResult UACFCharacterMovementComponent::CheckFloor() const 
{
	FHitResult Hit{};
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + FVector::DownVector * FloorCheckDistance;
	GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, ClimbQueryParams);
	return Hit;
}

bool UACFCharacterMovementComponent::TryClimbUpLedge() const 
{
	// TODO: Come back to this after putting in animations
	return false;
}
