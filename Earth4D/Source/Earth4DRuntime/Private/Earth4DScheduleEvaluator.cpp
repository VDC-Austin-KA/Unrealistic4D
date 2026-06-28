// Copyright Earth4D. Licensed for project use.
#include "Earth4DScheduleEvaluator.h"
#include "Earth4DSchedule.h"

namespace
{
	float Clamp01(float X) { return FMath::Clamp(X, 0.f, 1.f); }
	float Smooth(float X) { X = Clamp01(X); return X * X * (3.f - 2.f * X); }
	// Decelerating cubic ease-out (matches the web "ease").
	float Ease(float P) { P = Clamp01(P); const float k = 1.f - P; return 1.f - k * k * k; }

	FVector DirVec(EEarth4DDirection D)
	{
		switch (D)
		{
		case EEarth4DDirection::North: return FVector(0, 1, 0);
		case EEarth4DDirection::South: return FVector(0, -1, 0);
		case EEarth4DDirection::East:  return FVector(1, 0, 0);
		case EEarth4DDirection::West:  return FVector(-1, 0, 0);
		case EEarth4DDirection::Above: return FVector(0, 0, 1);
		case EEarth4DDirection::Below: return FVector(0, 0, -1);
		}
		return FVector(0, 0, 1);
	}

	/** Order an element id list for sequencing within a task. */
	void OrderIds(const UEarth4DSchedule& S, const FEarth4DTask& T, TArray<FString>& Ids)
	{
		switch (T.Sequence)
		{
		case EEarth4DSequence::ByElevation:
			Ids.Sort([&](const FString& A, const FString& B)
			{
				const FEarth4DElement* EA = S.FindElement(A);
				const FEarth4DElement* EB = S.FindElement(B);
				return (EA ? EA->BoundsCenter.Z : 0.f) < (EB ? EB->BoundsCenter.Z : 0.f);
			});
			break;
		case EEarth4DSequence::ByName:
			Ids.Sort([&](const FString& A, const FString& B)
			{
				const FEarth4DElement* EA = S.FindElement(A);
				const FEarth4DElement* EB = S.FindElement(B);
				return (EA ? EA->DisplayName : A) < (EB ? EB->DisplayName : B);
			});
			break;
		case EEarth4DSequence::Random:
			Ids.Sort([](const FString& A, const FString& B) { return GetTypeHash(A) < GetTypeHash(B); });
			break;
		case EEarth4DSequence::Together:
		default:
			break;
		}
	}

	/** Per-element sub-window [start,end] within the task (overlap / stagger aware). */
	void ComputeWindow(const FEarth4DTask& T, int32 Index, int32 Count, float& OutS, float& OutE)
	{
		const float Span = FMath::Max(T.End - T.Start, KINDA_SMALL_NUMBER);
		if (T.Stagger > 0.f)
		{
			OutS = T.Start + Index * T.Stagger;
			OutE = OutS + Span; // each element keeps the full per-element duration
			return;
		}
		if (T.Overlap >= 0.999f || Count <= 1)
		{
			OutS = T.Start; OutE = T.End; return;
		}
		const float L = Span / (1.f + (Count - 1) * (1.f - T.Overlap));
		const float Step = L * (1.f - T.Overlap);
		OutS = T.Start + Index * Step;
		OutE = OutS + L;
	}
}

namespace Earth4DScheduleEvaluator
{
	FEarth4DObjectState StyleState(EEarth4DAnimStyle Style, EEarth4DDirection Direction, float P,
		const FEarth4DElement& Element, float Distance)
	{
		FEarth4DObjectState St;
		const float E = Ease(P);
		const float K = 1.f - E;
		const float Op = Smooth(Clamp01(P / 0.55f));

		switch (Style)
		{
		case EEarth4DAnimStyle::Fade:
			St.Opacity = Ease(P);
			break;
		case EEarth4DAnimStyle::Drop:
		{
			const FVector V = DirVec(EEarth4DDirection::Above);
			St.Offset = V * (K * Distance); St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Rise:
		{
			const FVector V = DirVec(EEarth4DDirection::Below);
			St.Offset = V * (K * Distance); St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Slide:
		{
			const FVector V = DirVec(Direction);
			St.Offset = V * (K * Distance); St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Grow:
		{
			const float Sc = E;
			St.Scale = FVector(Sc); St.Offset = Element.BoundsCenter * (1.f - Sc); St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::GrowUp:
		{
			const float Sc = E;
			St.Scale = FVector(1, 1, Sc);
			// Anchor the base: shift down by the (negative) lower extent as it grows.
			St.Offset = FVector(0, 0, (Element.BoundsCenter.Z - 0.5f * Element.BoundsSize.Z) * (1.f - Sc));
			St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Spiral:
		{
			const FVector V = DirVec(EEarth4DDirection::Below);
			St.Offset = V * (K * Distance); St.Spin = K * PI * 1.5f; St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Swoop:
		{
			const FVector V = DirVec(Direction);
			St.Offset = V * (K * Distance) + FVector(0, 0, K * Distance * 0.5f);
			St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Assemble:
		{
			const FVector V = DirVec(Direction);
			const float Sc = 0.6f + 0.4f * E;
			St.Scale = FVector(Sc); St.Offset = V * (K * Distance * 0.5f); St.Opacity = Op; break;
		}
		case EEarth4DAnimStyle::Wipe:
		{
			const FVector V = DirVec(Direction);
			const FVector Center = Element.BoundsCenter;
			const float Ext = 0.5f * FMath::Abs(FVector::DotProduct(V.GetAbs(), Element.BoundsSize));
			const float C = FVector::DotProduct(Center, V);
			const float Front = C - Ext + 2.f * Ext * Ease(P) + 0.01f;
			St.bHasClip = true; St.ClipNormal = -V; St.ClipConstant = Front;
			break;
		}
		}
		return St;
	}

	void Evaluate(const UEarth4DSchedule& Schedule, float Day, TMap<FString, FEarth4DObjectState>& OutStates)
	{
		OutStates.Reset();

		// Build per-element affecting windows once.
		struct FAffect { const FEarth4DTask* Task; float S; float E; };
		TMap<FString, TArray<FAffect>> ByElement;
		for (const FEarth4DTask& T : Schedule.Tasks)
		{
			TArray<FString> Ids = T.ObjectIds;
			OrderIds(Schedule, T, Ids);
			for (int32 i = 0; i < Ids.Num(); ++i)
			{
				float S, E; ComputeWindow(T, i, Ids.Num(), S, E);
				ByElement.FindOrAdd(Ids[i]).Add({ &T, S, E });
			}
		}

		for (const FEarth4DElement& Elem : Schedule.Elements)
		{
			FEarth4DObjectState St; // defaults: visible, built
			const TArray<FAffect>* Affects = ByElement.Find(Elem.Id);

			if (Affects && Affects->Num() > 0)
			{
				// Governing = latest task that has already started.
				const FAffect* Gov = nullptr;
				bool bHasConstruction = false;
				for (const FAffect& A : *Affects)
				{
					if (A.Task->Type == EEarth4DTaskType::Construction) bHasConstruction = true;
					if (A.S <= Day && (!Gov || A.S > Gov->S)) Gov = &A;
				}

				if (!Gov)
				{
					// Nothing started yet: hidden if it will be constructed later.
					St.bVisible = !bHasConstruction;
					if (!St.bVisible) St.Opacity = 0.f;
				}
				else
				{
					const float P = FMath::Clamp((Day - Gov->S) / FMath::Max(Gov->E - Gov->S, KINDA_SMALL_NUMBER), 0.f, 1.f);
					if (Gov->Task->Type == EEarth4DTaskType::Demolition)
					{
						if (Day >= Gov->E) { St.bVisible = false; St.Opacity = 0.f; }
						else { St = StyleState(Gov->Task->Style, Gov->Task->Direction, 1.f - P, Elem, Gov->Task->Distance); }
					}
					else // construction / temporary build-in
					{
						if (Day >= Gov->E) { /* built: defaults */ }
						else { St = StyleState(Gov->Task->Style, Gov->Task->Direction, P, Elem, Gov->Task->Distance); }
					}
					if (P > 0.f && P < 1.f && Gov->Task->bHasGlowColor)
					{
						St.bHasGlow = true; St.GlowColor = Gov->Task->GlowColor; St.GlowIntensity = 2.f;
					}
				}
			}

			// Per-element appear/disappear gating + manual overrides.
			const FEarth4DObjectEdit& Ed = Elem.Edit;
			if (Ed.AppearDay >= 0.f && Day < Ed.AppearDay) { St.bVisible = false; St.Opacity = 0.f; }
			if (Ed.DisappearDay >= 0.f && Day >= Ed.DisappearDay) { St.bVisible = false; St.Opacity = 0.f; }
			if (Ed.bHidden) { St.bVisible = false; }
			St.Opacity *= Ed.Opacity;

			OutStates.Add(Elem.Id, St);
		}
	}
}
