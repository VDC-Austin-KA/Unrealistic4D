// Copyright Earth4D. Licensed for project use.
#include "Earth4DSchedule.h"

FEarth4DTask* UEarth4DSchedule::FindTask(const FString& Id)
{
	return Tasks.FindByPredicate([&](const FEarth4DTask& T) { return T.Id == Id; });
}

FEarth4DStage* UEarth4DSchedule::FindStage(const FString& Id)
{
	return Stages.FindByPredicate([&](const FEarth4DStage& S) { return S.Id == Id; });
}

FEarth4DElement* UEarth4DSchedule::FindElement(const FString& Id)
{
	return Elements.FindByPredicate([&](const FEarth4DElement& E) { return E.Id == Id; });
}

const FEarth4DElement* UEarth4DSchedule::FindElement(const FString& Id) const
{
	return Elements.FindByPredicate([&](const FEarth4DElement& E) { return E.Id == Id; });
}

void UEarth4DSchedule::GetBounds(float& OutMin, float& OutMax) const
{
	if (Tasks.Num() == 0) { OutMin = 0.f; OutMax = 1.f; return; }
	OutMin = TNumericLimits<float>::Max();
	OutMax = TNumericLimits<float>::Lowest();
	for (const FEarth4DTask& T : Tasks)
	{
		OutMin = FMath::Min(OutMin, T.Start);
		// Stagger pushes the last element's window past the authored end.
		const float StaggerExtra = T.Stagger * FMath::Max(0, T.ObjectIds.Num() - 1);
		OutMax = FMath::Max(OutMax, T.End + StaggerExtra);
	}
	if (OutMax <= OutMin) OutMax = OutMin + 1.f;
}

FEarth4DScheduleData UEarth4DSchedule::CaptureData() const
{
	FEarth4DScheduleData D;
	D.ProjectName = ProjectName;
	D.ProjectStartIso = ProjectStartIso;
	D.Stages = Stages;
	D.Tasks = Tasks;
	D.Elements = Elements;
	D.SelectionSets = SelectionSets;
	return D;
}

void UEarth4DSchedule::RestoreData(const FEarth4DScheduleData& D)
{
	ProjectName = D.ProjectName;
	ProjectStartIso = D.ProjectStartIso;
	Stages = D.Stages;
	Tasks = D.Tasks;
	Elements = D.Elements;
	SelectionSets = D.SelectionSets;
}
