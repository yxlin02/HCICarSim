// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDDynamicMeshGenerator.h"
#include "WorldBLDGeo.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ConstrainedDelaunay2.h"
#include "Polygon2.h"
#include "Curve/GeneralPolygon2.h"
#include "Arrangement2d.h"
#include "Algo/Reverse.h"

using namespace UE::Geometry;

namespace
{
	struct FDelaunayDisplacementParams
	{
		double Amount = 0.0;
		double Scale = 3000.0;
		int32 Seed = 0;
		TFunction<float(const FVector&)> InteriorDisplacementFunc;
		TFunction<FColor(const FVector&)> VertexColorFunc;
	};

	static void AddDelaunayTriangulatedPolygonImpl(
		UWorldBLDDynamicMeshGenerator* Generator,
		UMaterialInterface* Material,
		const TArray<FVector>& InVertices,
		const FDelaunayDisplacementParams* Displacement,
		float DuplicatePointEpsilon,
		bool bReverseWindingOrder)
	{
		if (!Generator)
		{
			return;
		}

		// 1) Copy and clean vertices (XY-plane assumed; Z preserved for output)
		TArray<FVector> Verts = InVertices;
		if (Verts.Num() < 3) return;

		const double Eps2 = FMath::Square((double)DuplicatePointEpsilon);
		TArray<FVector> Clean; Clean.Reserve(Verts.Num());
		for (const FVector& V : Verts)
		{
			if (Clean.Num() == 0 || (V - Clean.Last()).SizeSquared() > Eps2)
			{
				Clean.Add(V);
			}
		}
		if (Clean.Num() > 2 && (Clean[0] - Clean.Last()).SizeSquared() <= Eps2)
		{
			Clean.Pop();
		}
		if (Clean.Num() < 3) return;

		// Subdivide long boundary edges so triangulation has enough boundary resolution.
		// This prevents long cross-polygon triangles from becoming visible as ridges when Z varies along the loop.
		if (Generator->DelaunayGridSpacing > UE_SMALL_NUMBER)
		{
			TArray<FVector> Subdivided;
			Subdivided.Reserve(Clean.Num() * 2);
			for (int32 i = 0; i < Clean.Num(); ++i)
			{
				const FVector A3 = Clean[i];
				const FVector B3 = Clean[(i + 1) % Clean.Num()];
				Subdivided.Add(A3);

				const double EdgeLen2D = FVector2d(B3.X - A3.X, B3.Y - A3.Y).Length();
				const int32 NumSegs = FMath::Max(1, (int32)FMath::CeilToDouble(EdgeLen2D / Generator->DelaunayGridSpacing));
				for (int32 s = 1; s < NumSegs; ++s)
				{
					const double T = (double)s / (double)NumSegs;
					const FVector P = FMath::Lerp(A3, B3, (float)T);
					if ((P - Subdivided.Last()).SizeSquared() > Eps2)
					{
						Subdivided.Add(P);
					}
				}
			}

			// Re-close if the last point duplicated the first due to numerical lerp
			if (Subdivided.Num() > 2 && (Subdivided[0] - Subdivided.Last()).SizeSquared() <= Eps2)
			{
				Subdivided.Pop();
			}
			if (Subdivided.Num() >= 3)
			{
				Clean = MoveTemp(Subdivided);
			}
		}

		// 2) Convert to 2D polygon for Delaunay triangulation
		FPolygon2d Polygon;
		for (const FVector& V : Clean)
		{
			Polygon.AppendVertex(FVector2d(V.X, V.Y));
		}

		// 3) Set up Constrained Delaunay triangulation
		FConstrainedDelaunay2d Triangulator;
		FGeneralPolygon2d GeneralPolygon(Polygon);

		// Use Arrangement2d to build the constraint graph
		FArrangement2d Arrangement(Polygon.Bounds());
		Triangulator.FillRule = FConstrainedDelaunay2d::EFillRule::Odd;
		Triangulator.bOrientedEdges = true;
		Triangulator.bOutputCCW = true;
		Triangulator.bSplitBowties = false;

		// Add polygon edges as constraints
		for (FSegment2d Seg : GeneralPolygon.GetOuter().Segments())
		{
			Arrangement.Insert(Seg);
		}

		// Add interior vertices for increased mesh density
		// Calculate bounds and add a grid of interior points
		FAxisAlignedBox2d Bounds = Polygon.Bounds();
		const double GridSpacing = Generator->DelaunayGridSpacing;

		TArray<FVector2d> InteriorPoints;
		int32 NumOriginalVerts = Clean.Num();

		// Generate grid of interior points
		for (double X = Bounds.Min.X + GridSpacing; X < Bounds.Max.X; X += GridSpacing)
		{
			for (double Y = Bounds.Min.Y + GridSpacing; Y < Bounds.Max.Y; Y += GridSpacing)
			{
				FVector2d TestPoint(X, Y);
				// Check if point is inside the polygon
				if (Polygon.Contains(TestPoint))
				{
					InteriorPoints.Add(TestPoint);
					Arrangement.Graph.AppendVertex(TestPoint);
				}
			}
		}

		Triangulator.Add(Arrangement.Graph);

		// 4) Perform triangulation
		const bool bTriangulationSuccess = Triangulator.Triangulate();

		// Even if bTriangulationSuccess is false, there may still be some triangles
		if (Triangulator.Triangles.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("AddDelaunayTriangulatedPolygon: Triangulation produced no triangles"));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("AddDelaunayTriangulatedPolygon: Generated %d triangles from %d vertices (%d original, %d interior)"),
			Triangulator.Triangles.Num(), Triangulator.Vertices.Num(), NumOriginalVerts, InteriorPoints.Num());

		// 5) Convert triangulator output back to 3D vertices with boundary-respecting height interpolation
		TArray<FVector> OutVerts;
		OutVerts.Reserve(Triangulator.Vertices.Num());

		const double BoundaryMatchEpsilon = FMath::Max(1.0, (double)DuplicatePointEpsilon * 2.0);
		const double BoundaryMatchEpsilonSq = BoundaryMatchEpsilon * BoundaryMatchEpsilon;

		auto ClosestPointOnSegment2DSq = [](const FVector2d& P, const FVector2d& A, const FVector2d& B, double& OutT) -> double
		{
			const FVector2d AB = B - A;
			const double Denom = AB.SquaredLength();
			if (Denom <= UE_SMALL_NUMBER)
			{
				OutT = 0.0;
				return (P - A).SquaredLength();
			}
			OutT = ((P - A).Dot(AB)) / Denom;
			OutT = FMath::Clamp(OutT, 0.0, 1.0);
			const FVector2d Closest = A + OutT * AB;
			return (P - Closest).SquaredLength();
		};

		TArray<bool> bIsBoundaryVertex;
		bIsBoundaryVertex.Init(false, Triangulator.Vertices.Num());

		// Initialize with a best-fit plane to avoid a flat default for interior points
		// z = ax + by + c (least squares over boundary vertices)
		double SumX = 0.0, SumY = 0.0, SumZ = 0.0;
		double SumXX = 0.0, SumYY = 0.0, SumXY = 0.0;
		double SumXZ = 0.0, SumYZ = 0.0;
		const int32 N = Clean.Num();
		for (const FVector& BV : Clean)
		{
			const double X = (double)BV.X;
			const double Y = (double)BV.Y;
			const double Z = (double)BV.Z;
			SumX += X; SumY += Y; SumZ += Z;
			SumXX += X * X; SumYY += Y * Y; SumXY += X * Y;
			SumXZ += X * Z; SumYZ += Y * Z;
		}

		// Solve normal equations for (a,b,c)
		// [SumXX SumXY SumX] [a] = [SumXZ]
		// [SumXY SumYY SumY] [b]   [SumYZ]
		// [SumX  SumY  N   ] [c]   [SumZ ]
		auto Det3 = [](double a11, double a12, double a13, double a21, double a22, double a23, double a31, double a32, double a33) -> double
		{
			return a11 * (a22 * a33 - a23 * a32) - a12 * (a21 * a33 - a23 * a31) + a13 * (a21 * a32 - a22 * a31);
		};

		const double D = Det3(SumXX, SumXY, SumX, SumXY, SumYY, SumY, SumX, SumY, (double)N);
		double PlaneA = 0.0, PlaneB = 0.0, PlaneC = (N > 0) ? (SumZ / (double)N) : 0.0;
		if (FMath::Abs(D) > UE_SMALL_NUMBER)
		{
			const double Da = Det3(SumXZ, SumXY, SumX, SumYZ, SumYY, SumY, SumZ, SumY, (double)N);
			const double Db = Det3(SumXX, SumXZ, SumX, SumXY, SumYZ, SumY, SumX, SumZ, (double)N);
			const double Dc = Det3(SumXX, SumXY, SumXZ, SumXY, SumYY, SumYZ, SumX, SumY, SumZ);
			PlaneA = Da / D;
			PlaneB = Db / D;
			PlaneC = Dc / D;
		}

		for (int32 VIdx = 0; VIdx < Triangulator.Vertices.Num(); ++VIdx)
		{
			const FVector2d& V2D = Triangulator.Vertices[VIdx];
			double BestDistSq = TNumericLimits<double>::Max();
			double SecondBestDistSq = TNumericLimits<double>::Max();
			double BestT = 0.0;
			int32 BestSegStart = INDEX_NONE;

			for (int32 i = 0; i < Clean.Num(); ++i)
			{
				const FVector& A3 = Clean[i];
				const FVector& B3 = Clean[(i + 1) % Clean.Num()];
				const FVector2d A(A3.X, A3.Y);
				const FVector2d B(B3.X, B3.Y);
				double T = 0.0;
				double DistSq = ClosestPointOnSegment2DSq(V2D, A, B, T);
				if (DistSq < BestDistSq)
				{
					SecondBestDistSq = BestDistSq;
					BestDistSq = DistSq;
					BestT = T;
					BestSegStart = i;
				}
				else if (DistSq < SecondBestDistSq)
				{
					SecondBestDistSq = DistSq;
				}
			}

			float ZValue = (float)(PlaneA * (double)V2D.X + PlaneB * (double)V2D.Y + PlaneC);
			const bool bVeryCloseToBoundary = (BestDistSq <= (BoundaryMatchEpsilonSq * 1e-4));
			const bool bUnambiguousClosestSeg = (BestDistSq * 4.0 <= SecondBestDistSq);
			if (BestSegStart != INDEX_NONE && BestDistSq <= BoundaryMatchEpsilonSq && (bVeryCloseToBoundary || bUnambiguousClosestSeg))
			{
				const FVector& A3 = Clean[BestSegStart];
				const FVector& B3 = Clean[(BestSegStart + 1) % Clean.Num()];
				ZValue = (float)FMath::Lerp((double)A3.Z, (double)B3.Z, BestT);
				bIsBoundaryVertex[VIdx] = true;
			}

			OutVerts.Add(FVector(V2D.X, V2D.Y, ZValue));
		}

		// Smooth interior vertices by solving a discrete harmonic function (boundary vertices fixed)
		TArray<TArray<int32>> Adjacency;
		Adjacency.SetNum(Triangulator.Vertices.Num());
		for (const FIndex3i& Tri : Triangulator.Triangles)
		{
			const int32 A = Tri.A;
			const int32 B = Tri.B;
			const int32 C = Tri.C;
			Adjacency[A].AddUnique(B);
			Adjacency[A].AddUnique(C);
			Adjacency[B].AddUnique(A);
			Adjacency[B].AddUnique(C);
			Adjacency[C].AddUnique(A);
			Adjacency[C].AddUnique(B);
		}

		TArray<float> CurrentZ;
		TArray<float> NextZ;
		CurrentZ.SetNumUninitialized(OutVerts.Num());
		NextZ.SetNumUninitialized(OutVerts.Num());
		for (int32 i = 0; i < OutVerts.Num(); ++i)
		{
			CurrentZ[i] = OutVerts[i].Z;
			NextZ[i] = OutVerts[i].Z;
		}

		const int32 NumRelaxIters = 30;
		for (int32 Iter = 0; Iter < NumRelaxIters; ++Iter)
		{
			for (int32 VIdx = 0; VIdx < OutVerts.Num(); ++VIdx)
			{
				if (bIsBoundaryVertex[VIdx])
				{
					NextZ[VIdx] = CurrentZ[VIdx];
					continue;
				}

				const TArray<int32>& Neighbors = Adjacency[VIdx];
				if (Neighbors.Num() == 0)
				{
					NextZ[VIdx] = CurrentZ[VIdx];
					continue;
				}

				double WeightedSum = 0.0;
				double TotalWeight = 0.0;
				const FVector2d V2D(OutVerts[VIdx].X, OutVerts[VIdx].Y);
				for (int32 NIdx : Neighbors)
				{
					const FVector2d N2D(OutVerts[NIdx].X, OutVerts[NIdx].Y);
					double Dist = (V2D - N2D).Length();
					Dist = FMath::Max(Dist, 1.0);
					const double W = 1.0 / Dist;
					WeightedSum += (double)CurrentZ[NIdx] * W;
					TotalWeight += W;
				}

				NextZ[VIdx] = (TotalWeight > UE_SMALL_NUMBER) ? (float)(WeightedSum / TotalWeight) : CurrentZ[VIdx];
			}

			Swap(CurrentZ, NextZ);
		}

		for (int32 i = 0; i < OutVerts.Num(); ++i)
		{
			OutVerts[i].Z = CurrentZ[i];
		}

		// Optional: displacement for interior vertices (boundary unchanged)
		if (Displacement)
		{
			if (Displacement->InteriorDisplacementFunc)
			{
				// Custom callback-based displacement for interior vertices only
				for (int32 VIdx = 0; VIdx < OutVerts.Num(); ++VIdx)
				{
					if (bIsBoundaryVertex[VIdx])
					{
						continue;
					}
					OutVerts[VIdx].Z += Displacement->InteriorDisplacementFunc(OutVerts[VIdx]);
				}
			}
			else if (Displacement->Amount > UE_SMALL_NUMBER)
			{
				// Built-in upward-only Perlin displacement
				const double EffectiveScale = FMath::Max(1.0, Displacement->Scale);
				const double Frequency = 1.0 / EffectiveScale;

				FRandomStream Stream(Displacement->Seed);
				const double OffsetX = (double)Stream.FRandRange(-100000.0f, 100000.0f);
				const double OffsetY = (double)Stream.FRandRange(-100000.0f, 100000.0f);

				for (int32 VIdx = 0; VIdx < OutVerts.Num(); ++VIdx)
				{
					if (bIsBoundaryVertex[VIdx])
					{
						continue;
					}

					const FVector& V = OutVerts[VIdx];
					const FVector2D NoisePos((float)((V.X + OffsetX) * Frequency), (float)((V.Y + OffsetY) * Frequency));
					const float Noise = FMath::PerlinNoise2D(NoisePos); // [-1,1]
					const double UpOnly = (double)FMath::Max(0.0f, Noise); // [0,1]
					OutVerts[VIdx].Z += (float)(UpOnly * Displacement->Amount);
				}
			}
		}

		// 6) UVs from world-space coordinates with proper texture scale for consistent tiling
		TArray<FVector2D> UVs;
		UVs.Reserve(OutVerts.Num());

		// Use world-space coordinates divided by texture scale for consistent tiling
		for (const FVector& V : OutVerts)
		{
			UVs.Add(FVector2D(V.X * Generator->PolygonUVScaleMultiplier / Generator->TextureScale, V.Y * Generator->PolygonUVScaleMultiplier / Generator->TextureScale));
		}

		// 7) Convert triangulator indices to FIndex3i
		TArray<FIndex3i> Triangles;
		Triangles.Reserve(Triangulator.Triangles.Num());
		for (const FIndex3i& Tri : Triangulator.Triangles)
		{
			Triangles.Add(Tri);
		}

		// 8) Reverse triangle winding order if requested
		if (bReverseWindingOrder)
		{
			for (FIndex3i& Triangle : Triangles)
			{
				Swap(Triangle[1], Triangle[2]);
			}
		}

		// 9) Submit to mesh (with optional per-vertex colors)
		if (Displacement && Displacement->VertexColorFunc)
		{
			TArray<FColor> VertexColors;
			VertexColors.Reserve(OutVerts.Num());
			for (const FVector& V : OutVerts)
			{
				VertexColors.Add(Displacement->VertexColorFunc(V));
			}
			Generator->AddTriangles(Material, Triangles, OutVerts, UVs, VertexColors);
		}
		else
		{
			Generator->AddTriangles(Material, Triangles, OutVerts, UVs);
		}
	}
}

UWorldBLDDynamicMeshGenerator::UWorldBLDDynamicMeshGenerator()
{
	// Initialize mesh building infrastructure
	MeshDescription = MakeShareable(new FMeshDescription);
	MeshAttributes = MakeShareable(new FStaticMeshAttributes(*MeshDescription.Get()));
	MeshAttributes->Register();
	Builder = MakeShareable(new FMeshDescriptionBuilder);
	Builder->SetMeshDescription(MeshDescription.Get());
	Builder->EnablePolyGroups();
	Builder->SetNumUVLayers(1);
}

bool UWorldBLDDynamicMeshGenerator::CenterGeneratedMeshOnActorXY(AActor* TargetActor, FVector* OutWorldCenter)
{
	if (!IsValid(TargetActor) || !MeshDescription.IsValid() || !MeshAttributes.IsValid())
	{
		return false;
	}

	// No geometry to recenter.
	if (MeshDescription->Vertices().Num() == 0)
	{
		return false;
	}

	const FTransform OldActorTransform = TargetActor->GetActorTransform();
	const FVector OldActorLocation = OldActorTransform.GetLocation();

	// Compute world-space bounds of the current vertex positions (which are stored in mesh local space).
	const TVertexAttributesRef<FVector3f> VertexPositions = MeshAttributes->GetVertexPositions();
	FBox WorldBounds(ForceInit);

	for (const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs())
	{
		const FVector LocalPos = FVector(VertexPositions[VertexID]);
		const FVector WorldPos = OldActorTransform.TransformPosition(LocalPos);
		WorldBounds += WorldPos;
	}

	if (!WorldBounds.IsValid)
	{
		return false;
	}

	const FVector CenterWorld = WorldBounds.GetCenter();
	const FVector NewActorLocation(CenterWorld.X, CenterWorld.Y, OldActorLocation.Z);

	if (OutWorldCenter)
	{
		*OutWorldCenter = NewActorLocation;
	}

	// If we're already centered in XY, do nothing.
	const FVector DeltaXY(NewActorLocation.X - OldActorLocation.X, NewActorLocation.Y - OldActorLocation.Y, 0.0);
	if (DeltaXY.IsNearlyZero(KINDA_SMALL_NUMBER))
	{
		return false;
	}

	FTransform NewActorTransform = OldActorTransform;
	NewActorTransform.SetLocation(NewActorLocation);

	// Update mesh vertices so the final world-space geometry stays exactly in place.
	for (const FVertexID VertexID : MeshDescription->Vertices().GetElementIDs())
	{
		const FVector LocalPosOld = FVector(VertexPositions[VertexID]);
		const FVector WorldPos = OldActorTransform.TransformPosition(LocalPosOld);
		const FVector LocalPosNew = NewActorTransform.InverseTransformPosition(WorldPos);
		VertexPositions[VertexID] = FVector3f(LocalPosNew);
	}

	// Move the actor pivot to the bounds center in XY (Z preserved).
	TargetActor->SetActorTransform(NewActorTransform, false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

FPolygonGroupID UWorldBLDDynamicMeshGenerator::GetGroupID(UMaterialInterface* Material)
{
	// Defensive check - Builder should always be valid after construction, but verify to prevent crashes
	if (!Builder.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("GetGroupID: Builder is invalid! This should never happen - MeshGenerator may be corrupted."));
		// UE5.6+ deprecates FPolygonGroupID::Invalid; use INDEX_NONE as the invalid ID.
		return FPolygonGroupID(INDEX_NONE);
	}
	
	if (!PolygonGroups.Contains(Material))
	{
		FPolygonGroupID Group = Builder->AppendPolygonGroup(Material ? Material->GetFName() : NAME_None);
		PolygonGroups.Add(Material, Group);
	}
	return PolygonGroups[Material];
}

UStaticMesh* UWorldBLDDynamicMeshGenerator::CreateMesh(UObject* Outer, FName Name, EObjectFlags Flags)
{
#if WITH_EDITOR
	UStaticMesh* Mesh = nullptr;
	if (MeshDescription->Triangles().Num())
	{
		Mesh = NewObject<UStaticMesh>(Outer, Name, Flags);
		TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
		
		// Maintain deterministic material slot ordering that matches polygon group IDs.
		// TMap iteration order is not stable, and mismatched ordering will assign wrong materials to sections.
		struct FGroupMaterialPair
		{
			FPolygonGroupID GroupID;
			UMaterialInterface* Material = nullptr;
		};
		
		TArray<FGroupMaterialPair> SortedGroups;
		SortedGroups.Reserve(PolygonGroups.Num());
		for (const TPair<UMaterialInterface*, FPolygonGroupID>& KV : PolygonGroups)
		{
			FGroupMaterialPair Pair;
			Pair.GroupID = KV.Value;
			Pair.Material = KV.Key;
			SortedGroups.Add(Pair);
		}

		SortedGroups.Sort([](const FGroupMaterialPair& A, const FGroupMaterialPair& B)
		{
			return A.GroupID.GetValue() < B.GroupID.GetValue();
		});

		for (const FGroupMaterialPair& Entry : SortedGroups)
		{
			FStaticMaterial& StaticMaterial = StaticMaterials[StaticMaterials.AddDefaulted()];
			StaticMaterial.MaterialInterface = Entry.Material;
			StaticMaterial.MaterialSlotName = Entry.Material ? Entry.Material->GetFName() : NAME_None;
			StaticMaterial.UVChannelData = FMeshUVChannelInfo(1.f);
		}
		TArray<const FMeshDescription*> Descs = { MeshDescription.Get() };
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		// bFastBuild must be false to store mesh description source data, which is required
		// for proper serialization when the mesh is saved with the level
		Params.bFastBuild = false;
		Params.bAllowCpuAccess = true;
		Params.PerLODOverrides = { {true, true} };
		Mesh->BuildFromMeshDescriptions(Descs, Params);
		if (!Mesh->GetBodySetup())
			Mesh->CreateBodySetup();
		Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseComplexAsSimple;
	}
	return Mesh;
#else
	return nullptr;
#endif // WITH_EDITOR
}

void UWorldBLDDynamicMeshGenerator::AddTriangles(UMaterialInterface* Material, const TArray<FIndex3i>& Triangles,
                                             const TArray<FVector>& Vertices)
{
	// Call the overload with default UVs
	TArray<FVector2D> DefaultUVs;
	AddTriangles(Material, Triangles, Vertices, DefaultUVs);
}

void UWorldBLDDynamicMeshGenerator::AddTriangles(UMaterialInterface* Material, const TArray<FIndex3i>& Triangles,
                                             const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs)
{
	// Default to black vertex colors unless explicitly provided via the overload that accepts colors
	TArray<FColor> DefaultColors;
	AddTriangles(Material, Triangles, Vertices, UVs, DefaultColors);
}

void UWorldBLDDynamicMeshGenerator::AddTriangles(UMaterialInterface* Material, const TArray<FIndex3i>& Triangles,
	const TArray<FVector>& Vertices, const TArray<FVector2D>& UVs, const TArray<FColor>& VertexColors)
{
	FPolygonGroupID Group = GetGroupID(Material);

	// Defensive check - MeshAttributes should always be valid after construction
	if (!MeshAttributes.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("AddTriangles: MeshAttributes is invalid! This should never happen - MeshGenerator may be corrupted."));
		return;
	}
	TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = MeshAttributes->GetVertexInstanceColors();
	const FColor DefaultColor(0, 0, 0, 255);

	// === COMPUTE SMOOTH PER-VERTEX NORMALS ===
	// Accumulate face normals at each vertex (area-weighted) for smooth shading.
	// This is critical for surfaces with subtle curvature (e.g. road crown profiles)
	// where hardcoded up-normals would make the surface appear flat under lighting.
	TArray<FVector> VertexNormals;
	VertexNormals.SetNumZeroed(Vertices.Num());

	for (const FIndex3i& Tri : Triangles)
	{
		const FVector& A = Vertices[Tri.A];
		const FVector& B = Vertices[Tri.B];
		const FVector& C = Vertices[Tri.C];
		// Cross product gives area-weighted face normal (magnitude = 2x triangle area)
		const FVector FaceNormal = FVector::CrossProduct(B - A, C - A);
		VertexNormals[Tri.A] += FaceNormal;
		VertexNormals[Tri.B] += FaceNormal;
		VertexNormals[Tri.C] += FaceNormal;
	}

	for (FVector& N : VertexNormals)
	{
		if (!N.Normalize())
		{
			N = FVector::UpVector;
		}
	}
	
	// Add vertices
	TArray<FVertexID> VertexIDs;
	VertexIDs.Reserve(Vertices.Num());
	
	for (const FVector& Vertex : Vertices)
	{
		VertexIDs.Add(Builder->AppendVertex(Vertex));
	}
	
	// Add triangles with instances
	TArray<FVertexInstanceID> InstanceIDs;
	InstanceIDs.Reserve(Triangles.Num() * 3);
	
	double UVScale = 0.01;
	
	for (int32 i = 0; i < Triangles.Num(); i++)
	{
		const FIndex3i& Triangle = Triangles[i];
		
		for (int32 j = 0; j < 3; j++)
		{
			int32 Index = i * 3 + j;
			int32 VertexIndex = Triangle[j];
			
			InstanceIDs.Add(Builder->AppendInstance(VertexIDs[VertexIndex]));
			Builder->SetInstanceNormal(InstanceIDs[Index], VertexNormals[VertexIndex]);
			
			// Set UVs - use provided UVs if available, otherwise use vertex position
			if (UVs.IsValidIndex(VertexIndex))
			{
				Builder->SetInstanceUV(InstanceIDs[Index], UVs[VertexIndex], 0);
			}
			else
			{
				// Default UVs based on vertex position
				Builder->SetInstanceUV(InstanceIDs[Index], FVector2D(Vertices[VertexIndex]) * UVScale, 0);
			}

			// Set vertex instance color - default black unless an explicit per-vertex color was provided
			const FColor UseColor = VertexColors.IsValidIndex(VertexIndex) ? VertexColors[VertexIndex] : DefaultColor;
			// FLinearColor::ToFVector4f() is not available in UE5.6; explicitly convert to FVector4f.
			const FLinearColor LinearColor(UseColor);
			VertexInstanceColors[InstanceIDs[Index]] = FVector4f(
				(float)LinearColor.R,
				(float)LinearColor.G,
				(float)LinearColor.B,
				(float)LinearColor.A);
		}
		
		Builder->AppendTriangle(InstanceIDs[i * 3 + 0], InstanceIDs[i * 3 + 1], InstanceIDs[i * 3 + 2], Group);
	}
	
}

void UWorldBLDDynamicMeshGenerator::AddTriangulatedPolygon(FTransform Transform, TArray<FVector> PolygonPoints, bool bReverseWindingOrder)
{
	// Use simple ear clipping triangulation for polygons without material
	// Ensure consistent CCW winding in XY so normals face +Z
	auto ComputeSignedAreaXY = [](const TArray<FVector>& Points) -> double
	{
		if (Points.Num() < 3) return 0.0;
		double TwiceArea = 0.0;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FVector& A = Points[i];
			const FVector& B = Points[(i + 1) % Points.Num()];
			TwiceArea += (double)A.X * (double)B.Y - (double)B.X * (double)A.Y;
		}
		return 0.5 * TwiceArea;
	};

	// If perimeter is clockwise in XY (negative area), reverse to CCW
	if (ComputeSignedAreaXY(PolygonPoints) < 0.0)
	{
		Algo::Reverse(PolygonPoints);
	}
	
	// Transform points
	TArray<FVector> TransformedPoints;
	TransformedPoints.Reserve(PolygonPoints.Num());
	for (const FVector& Point : PolygonPoints)
	{
		TransformedPoints.Add(Transform.TransformPosition(Point));
	}
	
	// Use robust triangulation with default material
	AddRobustTriangulatedPolygon(nullptr, TransformedPoints, 0.5f, 1.0f, 5.0f, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddTriangulatedPolygon(UMaterialInterface* Material, FTransform Transform, TArray<FVector> PolygonPoints, bool bReverseWindingOrder)
{
	// Ensure consistent CCW winding in XY so normals face +Z
	auto ComputeSignedAreaXY = [](const TArray<FVector>& Points) -> double
	{
		if (Points.Num() < 3) return 0.0;
		double TwiceArea = 0.0;
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			const FVector& A = Points[i];
			const FVector& B = Points[(i + 1) % Points.Num()];
			TwiceArea += (double)A.X * (double)B.Y - (double)B.X * (double)A.Y;
		}
		return 0.5 * TwiceArea;
	};

	if (ComputeSignedAreaXY(PolygonPoints) < 0.0)
	{
		Algo::Reverse(PolygonPoints);
	}
	
	// Transform points
	TArray<FVector> TransformedPoints;
	TransformedPoints.Reserve(PolygonPoints.Num());
	for (const FVector& Point : PolygonPoints)
	{
		TransformedPoints.Add(Transform.TransformPosition(Point));
	}
	
	// Use robust triangulation with the specified material
	AddRobustTriangulatedPolygon(Material, TransformedPoints, 0.5f, 1.0f, 5.0f, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddDelaunayTriangulatedPolygon(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	float DuplicatePointEpsilon, bool bReverseWindingOrder)
{
	AddDelaunayTriangulatedPolygonImpl(this, Material, InVertices, nullptr, DuplicatePointEpsilon, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddDelaunayTriangulatedPolygonDisplaced(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	double DisplacementAmount, double DisplacementScale, int32 Seed, float DuplicatePointEpsilon, bool bReverseWindingOrder)
{
	FDelaunayDisplacementParams Params;
	Params.Amount = DisplacementAmount;
	Params.Scale = DisplacementScale;
	Params.Seed = Seed;
	AddDelaunayTriangulatedPolygonImpl(this, Material, InVertices, &Params, DuplicatePointEpsilon, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddDelaunayTriangulatedPolygonWithCallback(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	TFunctionRef<float(const FVector&)> InteriorDisplacementFunc, float DuplicatePointEpsilon, bool bReverseWindingOrder)
{
	FDelaunayDisplacementParams Params;
	Params.InteriorDisplacementFunc = [&InteriorDisplacementFunc](const FVector& V) { return InteriorDisplacementFunc(V); };
	AddDelaunayTriangulatedPolygonImpl(this, Material, InVertices, &Params, DuplicatePointEpsilon, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddDelaunayTriangulatedPolygonWithCallbackColored(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	TFunctionRef<float(const FVector&)> InteriorDisplacementFunc, TFunctionRef<FColor(const FVector&)> VertexColorFunc,
	float DuplicatePointEpsilon, bool bReverseWindingOrder)
{
	FDelaunayDisplacementParams Params;
	Params.InteriorDisplacementFunc = [&InteriorDisplacementFunc](const FVector& V) { return InteriorDisplacementFunc(V); };
	Params.VertexColorFunc = [&VertexColorFunc](const FVector& V) { return VertexColorFunc(V); };
	AddDelaunayTriangulatedPolygonImpl(this, Material, InVertices, &Params, DuplicatePointEpsilon, bReverseWindingOrder);
}

void UWorldBLDDynamicMeshGenerator::AddDelaunayTriangulatedPolygonColored(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	TFunctionRef<FColor(const FVector&)> VertexColorFunc, float DuplicatePointEpsilon, bool bReverseWindingOrder)
{
	// 1) Copy and clean vertices (XY-plane assumed; Z preserved for output)
	TArray<FVector> Verts = InVertices;
	if (Verts.Num() < 3) return;

	const double Eps2 = FMath::Square((double)DuplicatePointEpsilon);
	TArray<FVector> Clean; Clean.Reserve(Verts.Num());
	for (const FVector& V : Verts)
	{
		if (Clean.Num() == 0 || (V - Clean.Last()).SizeSquared() > Eps2)
		{
			Clean.Add(V);
		}
	}
	if (Clean.Num() > 2 && (Clean[0] - Clean.Last()).SizeSquared() <= Eps2)
	{
		Clean.Pop();
	}
	if (Clean.Num() < 3) return;

	// Subdivide long boundary edges so triangulation has enough boundary resolution.
	// This prevents long cross-polygon triangles from becoming visible as ridges when Z varies along the loop.
	if (DelaunayGridSpacing > UE_SMALL_NUMBER)
	{
		TArray<FVector> Subdivided;
		Subdivided.Reserve(Clean.Num() * 2);
		for (int32 i = 0; i < Clean.Num(); ++i)
		{
			const FVector A3 = Clean[i];
			const FVector B3 = Clean[(i + 1) % Clean.Num()];
			Subdivided.Add(A3);

			const double EdgeLen2D = FVector2d(B3.X - A3.X, B3.Y - A3.Y).Length();
			const int32 NumSegs = FMath::Max(1, (int32)FMath::CeilToDouble(EdgeLen2D / DelaunayGridSpacing));
			for (int32 s = 1; s < NumSegs; ++s)
			{
				const double T = (double)s / (double)NumSegs;
				const FVector P = FMath::Lerp(A3, B3, (float)T);
				if ((P - Subdivided.Last()).SizeSquared() > Eps2)
				{
					Subdivided.Add(P);
				}
			}
		}

		// Re-close if the last point duplicated the first due to numerical lerp
		if (Subdivided.Num() > 2 && (Subdivided[0] - Subdivided.Last()).SizeSquared() <= Eps2)
		{
			Subdivided.Pop();
		}
		if (Subdivided.Num() >= 3)
		{
			Clean = MoveTemp(Subdivided);
		}
	}

	// 2) Convert to 2D polygon for Delaunay triangulation
	FPolygon2d Polygon;
	for (const FVector& V : Clean)
	{
		Polygon.AppendVertex(FVector2d(V.X, V.Y));
	}

	// 3) Set up Constrained Delaunay triangulation
	FConstrainedDelaunay2d Triangulator;
	FGeneralPolygon2d GeneralPolygon(Polygon);

	// Use Arrangement2d to build the constraint graph
	FArrangement2d Arrangement(Polygon.Bounds());
	Triangulator.FillRule = FConstrainedDelaunay2d::EFillRule::Odd;
	Triangulator.bOrientedEdges = true;
	Triangulator.bOutputCCW = true;
	Triangulator.bSplitBowties = false;

	// Add polygon edges as constraints
	for (FSegment2d Seg : GeneralPolygon.GetOuter().Segments())
	{
		Arrangement.Insert(Seg);
	}

	// Add interior vertices for increased mesh density
	FAxisAlignedBox2d Bounds = Polygon.Bounds();
	const double GridSpacing = DelaunayGridSpacing;

	TArray<FVector2d> InteriorPoints;
	int32 NumOriginalVerts = Clean.Num();

	for (double X = Bounds.Min.X + GridSpacing; X < Bounds.Max.X; X += GridSpacing)
	{
		for (double Y = Bounds.Min.Y + GridSpacing; Y < Bounds.Max.Y; Y += GridSpacing)
		{
			FVector2d TestPoint(X, Y);
			if (Polygon.Contains(TestPoint))
			{
				InteriorPoints.Add(TestPoint);
				Arrangement.Graph.AppendVertex(TestPoint);
			}
		}
	}

	Triangulator.Add(Arrangement.Graph);

	// 4) Perform triangulation
	const bool bTriangulationSuccess = Triangulator.Triangulate();

	if (Triangulator.Triangles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("AddDelaunayTriangulatedPolygonColored: Triangulation produced no triangles"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AddDelaunayTriangulatedPolygonColored: Generated %d triangles from %d vertices (%d original, %d interior)"),
		Triangulator.Triangles.Num(), Triangulator.Vertices.Num(), NumOriginalVerts, InteriorPoints.Num());

	// 5) Convert triangulator output back to 3D vertices with boundary-respecting height interpolation
	TArray<FVector> OutVerts;
	OutVerts.Reserve(Triangulator.Vertices.Num());

	const double BoundaryMatchEpsilon = FMath::Max(1.0, (double)DuplicatePointEpsilon * 2.0);
	const double BoundaryMatchEpsilonSq = BoundaryMatchEpsilon * BoundaryMatchEpsilon;

	auto ClosestPointOnSegment2DSq = [](const FVector2d& P, const FVector2d& A, const FVector2d& B, double& OutT) -> double
	{
		const FVector2d AB = B - A;
		const double Denom = AB.SquaredLength();
		if (Denom <= UE_SMALL_NUMBER)
		{
			OutT = 0.0;
			return (P - A).SquaredLength();
		}
		OutT = ((P - A).Dot(AB)) / Denom;
		OutT = FMath::Clamp(OutT, 0.0, 1.0);
		const FVector2d Closest = A + OutT * AB;
		return (P - Closest).SquaredLength();
	};

	TArray<bool> bIsBoundaryVertex;
	bIsBoundaryVertex.Init(false, Triangulator.Vertices.Num());

	// Initialize with a best-fit plane to avoid a flat default for interior points
	// z = ax + by + c (least squares over boundary vertices)
	double SumX = 0.0, SumY = 0.0, SumZ = 0.0;
	double SumXX = 0.0, SumYY = 0.0, SumXY = 0.0;
	double SumXZ = 0.0, SumYZ = 0.0;
	const int32 N = Clean.Num();
	for (const FVector& BV : Clean)
	{
		SumX += (double)BV.X; SumY += (double)BV.Y; SumZ += (double)BV.Z;
		SumXX += (double)BV.X * (double)BV.X;
		SumYY += (double)BV.Y * (double)BV.Y;
		SumXY += (double)BV.X * (double)BV.Y;
		SumXZ += (double)BV.X * (double)BV.Z;
		SumYZ += (double)BV.Y * (double)BV.Z;
	}

	const double Det = SumXX * SumYY * (double)N + 2.0 * SumXY * SumX * SumY - SumXX * SumY * SumY - SumYY * SumX * SumX - (double)N * SumXY * SumXY;
	double PlaneA = 0.0, PlaneB = 0.0, PlaneC = (N > 0) ? (SumZ / (double)N) : 0.0;
	const bool bPlaneValid = FMath::Abs(Det) > UE_SMALL_NUMBER;
	if (bPlaneValid)
	{
		// Solve normal equations for a,b,c
		// [SumXX SumXY SumX][a] = [SumXZ]
		// [SumXY SumYY SumY][b]   [SumYZ]
		// [SumX  SumY  N   ][c]   [SumZ ]
		const double InvDet = 1.0 / Det;
		PlaneA = InvDet * (SumXZ * (SumYY * (double)N - SumY * SumY) + SumYZ * (SumX * SumY - SumXY * (double)N) + SumZ * (SumXY * SumY - SumYY * SumX));
		PlaneB = InvDet * (SumXZ * (SumX * SumY - SumXY * (double)N) + SumYZ * (SumXX * (double)N - SumX * SumX) + SumZ * (SumXY * SumX - SumXX * SumY));
		PlaneC = InvDet * (SumXZ * (SumXY * SumY - SumYY * SumX) + SumYZ * (SumXY * SumX - SumXX * SumY) + SumZ * (SumXX * SumYY - SumXY * SumXY));
	}

	for (int32 Vi = 0; Vi < Triangulator.Vertices.Num(); ++Vi)
	{
		const FVector2d V2 = Triangulator.Vertices[Vi];

		// Attempt boundary match
		int32 BestBoundaryIndexA = -1;
		int32 BestBoundaryIndexB = -1;
		double BestSegDistSq = TNumericLimits<double>::Max();
		double BestSegT = 0.0;

		for (int32 i = 0; i < Clean.Num(); ++i)
		{
			const FVector2d A2(Clean[i].X, Clean[i].Y);
			const FVector2d B2(Clean[(i + 1) % Clean.Num()].X, Clean[(i + 1) % Clean.Num()].Y);
			double SegT = 0.0;
			const double DistSq = ClosestPointOnSegment2DSq(V2, A2, B2, SegT);
			if (DistSq < BestSegDistSq)
			{
				BestSegDistSq = DistSq;
				BestSegT = SegT;
				BestBoundaryIndexA = i;
				BestBoundaryIndexB = (i + 1) % Clean.Num();
			}
		}

		float OutZ = 0.0f;
		if (BestSegDistSq <= BoundaryMatchEpsilonSq && BestBoundaryIndexA >= 0 && BestBoundaryIndexB >= 0)
		{
			// Interpolate Z along closest boundary segment
			const float ZA = Clean[BestBoundaryIndexA].Z;
			const float ZB = Clean[BestBoundaryIndexB].Z;
			OutZ = FMath::Lerp(ZA, ZB, (float)BestSegT);
			bIsBoundaryVertex[Vi] = true;
		}
		else
		{
			// Pure-math fallback: best fit plane
			if (bPlaneValid)
			{
				OutZ = static_cast<float>(PlaneA * static_cast<double>(V2.X) + PlaneB * static_cast<double>(V2.Y) + PlaneC);
			}
			else
			{
				OutZ = static_cast<float>(PlaneC);
			}
		}

		OutVerts.Add(FVector((float)V2.X, (float)V2.Y, OutZ));
	}

	// 6) Smooth interior Z values to reduce artifacts (keep boundary fixed)
	TArray<float> CurrentZ; CurrentZ.Reserve(OutVerts.Num());
	TArray<float> NextZ; NextZ.Reserve(OutVerts.Num());
	for (const FVector& V : OutVerts)
	{
		CurrentZ.Add(V.Z);
		NextZ.Add(V.Z);
	}

	// Build adjacency (2D) from triangles
	TArray<TArray<int32>> Adjacency;
	Adjacency.SetNum(OutVerts.Num());
	for (const FIndex3i& Tri : Triangulator.Triangles)
	{
		const int32 A = Tri.A, B = Tri.B, C = Tri.C;
		Adjacency[A].AddUnique(B); Adjacency[A].AddUnique(C);
		Adjacency[B].AddUnique(A); Adjacency[B].AddUnique(C);
		Adjacency[C].AddUnique(A); Adjacency[C].AddUnique(B);
	}

	constexpr int32 NumIterations = 4;
	for (int32 Iter = 0; Iter < NumIterations; ++Iter)
	{
		for (int32 VIdx = 0; VIdx < OutVerts.Num(); ++VIdx)
		{
			if (bIsBoundaryVertex[VIdx])
			{
				NextZ[VIdx] = CurrentZ[VIdx];
				continue;
			}

			const TArray<int32>& Neighbors = Adjacency[VIdx];
			if (Neighbors.Num() == 0)
			{
				NextZ[VIdx] = CurrentZ[VIdx];
				continue;
			}

			double WeightedSum = 0.0;
			double TotalWeight = 0.0;

			const FVector2d V2D(OutVerts[VIdx].X, OutVerts[VIdx].Y);
			for (int32 NIdx : Neighbors)
			{
				const FVector2d N2D(OutVerts[NIdx].X, OutVerts[NIdx].Y);
				double Dist = (V2D - N2D).Length();
				Dist = FMath::Max(Dist, 1.0);
				const double W = 1.0 / Dist;
				WeightedSum += (double)CurrentZ[NIdx] * W;
				TotalWeight += W;
			}

			NextZ[VIdx] = (TotalWeight > UE_SMALL_NUMBER) ? (float)(WeightedSum / TotalWeight) : CurrentZ[VIdx];
		}

		Swap(CurrentZ, NextZ);
	}

	for (int32 i = 0; i < OutVerts.Num(); ++i)
	{
		OutVerts[i].Z = CurrentZ[i];
	}

	// 7) UVs from world-space coordinates with proper texture scale for consistent tiling
	TArray<FVector2D> UVs;
	UVs.Reserve(OutVerts.Num());
	for (const FVector& V : OutVerts)
	{
		UVs.Add(FVector2D(V.X * PolygonUVScaleMultiplier / TextureScale, V.Y * PolygonUVScaleMultiplier / TextureScale));
	}

	// 8) Convert triangulator indices to FIndex3i
	TArray<FIndex3i> Triangles;
	Triangles.Reserve(Triangulator.Triangles.Num());
	for (const FIndex3i& Tri : Triangulator.Triangles)
	{
		Triangles.Add(Tri);
	}

	// 9) Reverse triangle winding order if requested
	if (bReverseWindingOrder)
	{
		for (FIndex3i& Triangle : Triangles)
		{
			Swap(Triangle[1], Triangle[2]);
		}
	}

	// 10) Colors
	TArray<FColor> VertexColors;
	VertexColors.Reserve(OutVerts.Num());
	for (const FVector& V : OutVerts)
	{
		VertexColors.Add(VertexColorFunc(V));
	}

	// 11) Submit to mesh
	AddTriangles(Material, Triangles, OutVerts, UVs, VertexColors);
}

void UWorldBLDDynamicMeshGenerator::AddRobustTriangulatedPolygon(UMaterialInterface* Material, const TArray<FVector>& InVertices,
	float DuplicatePointEpsilon, float MinEdgeLength, float MinSpikeAngleDeg, bool bReverseWindingOrder)
{
	// 1) Copy and clean vertices (XY-plane assumed; Z preserved for output)
	TArray<FVector> Verts = InVertices;
	if (Verts.Num() < 3) return;

	const double Eps2 = FMath::Square((double)DuplicatePointEpsilon);
	TArray<FVector> Clean; Clean.Reserve(Verts.Num());
	for (const FVector& V : Verts)
	{
		if (Clean.Num() == 0 || (V - Clean.Last()).SizeSquared() > Eps2)
		{
			Clean.Add(V);
		}
	}
	if (Clean.Num() > 2 && (Clean[0] - Clean.Last()).SizeSquared() <= Eps2)
	{
		Clean.Pop();
	}
	// Remove tiny edges
	if (MinEdgeLength > 0.0f)
	{
		TArray<FVector> Filtered; Filtered.Reserve(Clean.Num());
		for (int32 i = 0; i < Clean.Num(); ++i)
		{
			const FVector A = Clean[i];
			const FVector B = Clean[(i + 1) % Clean.Num()];
			if ((B - A).SizeSquared() >= FMath::Square(MinEdgeLength))
			{
				Filtered.Add(A);
			}
		}
		Clean = Filtered;
	}
	if (Clean.Num() < 3) return;

	// Remove narrow spikes by angle threshold
	if (MinSpikeAngleDeg > 0.0f && Clean.Num() >= 3)
	{
		const double MinAngleRad = FMath::DegreesToRadians((double)MinSpikeAngleDeg);
		bool bChanged = true; int32 SafeIter = 0;
		while (bChanged && Clean.Num() >= 3 && SafeIter++ < 64)
		{
			bChanged = false;
			for (int32 i = 0; i < Clean.Num(); ++i)
			{
				const FVector A = Clean[(i - 1 + Clean.Num()) % Clean.Num()];
				const FVector B = Clean[i];
				const FVector C = Clean[(i + 1) % Clean.Num()];
				FVector2D a(A.X, A.Y), b(B.X, B.Y), c(C.X, C.Y);
				FVector2D u = (a - b).GetSafeNormal();
				FVector2D v = (c - b).GetSafeNormal();
				double CosAng = FMath::Clamp((double)(u.X * v.X + u.Y * v.Y), -1.0, 1.0);
				double Angle = FMath::Acos(CosAng);
				if (Angle < MinAngleRad)
				{
					Clean.RemoveAt(i);
					bChanged = true;
					break;
				}
			}
		}
	}
	if (Clean.Num() < 3) return;

	// Ensure CCW orientation
	double Area = 0.0;
	for (int32 i = 0, n = Clean.Num(); i < n; ++i)
	{
		const FVector2D a(Clean[i].X, Clean[i].Y);
		const FVector2D b(Clean[(i + 1) % n].X, Clean[(i + 1) % n].Y);
		Area += (double)a.X * b.Y - (double)a.Y * b.X;
	}
	if (Area < 0.0)
	{
		Algo::Reverse(Clean);
	}

	// 2) Triangulate with robust ear clipping in XY
	TArray<FIndex3i> Triangles;
	{
		TArray<FVector2D> Poly2D; Poly2D.Reserve(Clean.Num());
		for (const FVector& V : Clean) Poly2D.Add(FVector2D(V.X, V.Y));

		auto IsConvex = [&](const FVector2D& a, const FVector2D& b, const FVector2D& c) -> bool
		{
			const double Cross = (double)(b.X - a.X) * (c.Y - a.Y) - (double)(b.Y - a.Y) * (c.X - a.X);
			return Cross > 0.0; // assumes CCW
		};

		auto PointInTri = [&](const FVector2D& p, const FVector2D& a, const FVector2D& b, const FVector2D& c) -> bool
		{
			auto Sign = [](const FVector2D& p1, const FVector2D& p2, const FVector2D& p3)
			{
				return (p1.X - p3.X) * (p2.Y - p3.Y) - (p2.X - p3.X) * (p1.Y - p3.Y);
			};
			double d1 = Sign(p, a, b);
			double d2 = Sign(p, b, c);
			double d3 = Sign(p, c, a);
			bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
			bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
			return !(has_neg && has_pos);
		};

		TArray<int32> Index; Index.Reserve(Poly2D.Num());
		for (int32 i = 0; i < Poly2D.Num(); ++i) Index.Add(i);

		int32 Safe = 0;
		while (Index.Num() >= 3 && Safe++ < 10000)
		{
			bool bClipped = false;
			for (int32 i = 0; i < Index.Num(); ++i)
			{
				int32 i0 = Index[(i - 1 + Index.Num()) % Index.Num()];
				int32 i1 = Index[i];
				int32 i2 = Index[(i + 1) % Index.Num()];
				const FVector2D& A = Poly2D[i0];
				const FVector2D& B = Poly2D[i1];
				const FVector2D& C = Poly2D[i2];

				if (!IsConvex(A, B, C)) continue;

				bool bContains = false;
				for (int32 j = 0; j < Index.Num(); ++j)
				{
					int32 k = Index[j];
					if (k == i0 || k == i1 || k == i2) continue;
					if (PointInTri(Poly2D[k], A, B, C)) { bContains = true; break; }
				}
				if (bContains) continue;

				Triangles.Add(FIndex3i(i0, i1, i2));
				Index.RemoveAt(i);
				bClipped = true;
				break;
			}
			if (!bClipped) break; // fallback out
		}
	}

	if (Triangles.Num() == 0) return;

	// 3) Output vertices are the cleaned inputs
	TArray<FVector> OutVerts = Clean;

	// 4) UVs from world-space coordinates with proper texture scale for consistent tiling
	TArray<FVector2D> UVs;
	UVs.Reserve(OutVerts.Num());
	
	// Use world-space coordinates divided by texture scale for consistent tiling
	for (const FVector& V : OutVerts)
	{
		UVs.Add(FVector2D(V.X * PolygonUVScaleMultiplier / TextureScale, V.Y * PolygonUVScaleMultiplier / TextureScale));
	}

	// 5) Reverse triangle winding order if requested
	if (bReverseWindingOrder)
	{
		for (FIndex3i& Triangle : Triangles)
		{
			Swap(Triangle[1], Triangle[2]);
		}
	}

	// 6) Submit to mesh
	AddTriangles(Material, Triangles, OutVerts, UVs);
}
