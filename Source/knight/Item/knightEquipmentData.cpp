#include "Item/knightEquipmentData.h"

const UknightMaintenanceCatalog* UknightEquipmentSettings::LoadCatalog()
{
	return GetDefault<UknightEquipmentSettings>()->Catalog.LoadSynchronous();
}
