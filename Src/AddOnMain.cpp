#include "APIEnvir.h"
#include "ACAPinc.h"

#include "ResourceIds.hpp"
#include "RS.hpp"
#include "DGModule.hpp"
#include "UniString.hpp"
#include "ObjectState.hpp"
#include "StringConversion.hpp"
#include "API_Guid.hpp"
#include "APIdefs_AttributeIndex.hpp"
#include "Owner.hpp"
#include <cmath>

static const GSResID AddOnInfoID         = ID_ADDON_INFO;
static const Int32   AddOnNameID         = 1;
static const Int32   AddOnDescriptionID  = 2;
static const short   AddOnMenuID         = ID_ADDON_MENU;
static const Int32   AddOnCommandID      = 1;

// =============================================================================
// Helper Utilities
// =============================================================================

static API_AttributeIndex FindOrCreateLayerByName (const GS::UniString& layerName)
{
	if (layerName.IsEmpty ()) {
		return APIApplicationLayerAttributeIndex;
	}

	API_Attr_Head attrHead {};
	attrHead.typeID = API_LayerID;
	GS::UniString searchName = layerName;
	attrHead.uniStringNamePtr = &searchName;
	if (ACAPI_Attribute_Search (&attrHead) == NoError) {
		return attrHead.index;
	}

	API_Attribute attrib {};
	attrib.header.typeID = API_LayerID;
	attrib.header.uniStringNamePtr = &searchName;
	if (ACAPI_Attribute_Create (&attrib, nullptr) == NoError) {
		return attrib.header.index;
	}

	return APIInvalidAttributeIndex;
}

// =============================================================================
// Superquadra Base Command
// =============================================================================

class SuperquadraBaseCommand : public API_AddOnCommand {
public:
	virtual GS::String GetNamespace () const override
	{
		return "Superquadra";
	}

	virtual GS::Optional<GS::UniString> GetSchemaDefinitions () const override
	{
		return GS::NoValue;
	}

	virtual GS::Optional<GS::UniString> GetInputParametersSchema () const override
	{
		return GS::NoValue;
	}

	virtual GS::Optional<GS::UniString> GetResponseSchema () const override
	{
		return GS::NoValue;
	}

	virtual API_AddOnCommandExecutionPolicy GetExecutionPolicy () const override
	{
		return API_AddOnCommandExecutionPolicy::ScheduleForExecutionOnMainThread;
	}

	virtual bool IsProcessWindowVisible () const override
	{
		return false;
	}

	virtual void OnResponseValidationFailed (const GS::ObjectState& /*response*/) const override
	{
	}
};

// =============================================================================
// 1. Ping Command
// =============================================================================

class PingCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "Ping";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		res.Add ("status", "ok");
		res.Add ("addOn", "SuperquadraAutomation");
		res.Add ("version", "2.0.0");
		res.Add ("accessLevel", 5);
		res.Add ("message", "Superquadra Native C++ Kernel v2.0.0 connected to Archicad 29.");

		GS::Array<GS::UniString> commands;
		commands.Push ("Ping");
		commands.Push ("ChangeElementsLayer");
		commands.Push ("ManageHotlinks");
		commands.Push ("DeleteElements");
		commands.Push ("ListLayers");
		commands.Push ("ModifyLayersState");
		commands.Push ("PublishSet");
		commands.Push ("ListLayoutDatabases");
		commands.Push ("PlaceDrawingOnLayout");
		commands.Push ("SetFloorPlanCutPlane");
		commands.Push ("SolidElementOperations");
		commands.Push ("ModifyElementsGeometry");
		commands.Push ("CreateNativeElements");
		commands.Push ("GetSetGDLParameters");
		commands.Push ("DatabaseAndViewControl");
		res.Add ("commands", commands);
		return res;
	}
};

// =============================================================================
// 2. ChangeElementsLayer Command
// =============================================================================

class ChangeElementsLayerCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ChangeElementsLayer";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString targetLayerName;
		if (!parameters.Get ("layerName", targetLayerName) || targetLayerName.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "layerName parameter is required");
			return res;
		}

		GS::Array<GS::UniString> elements;
		if (!parameters.Get ("elements", elements) || elements.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "elements array parameter is required");
			return res;
		}

		API_AttributeIndex targetLayerIndex = FindOrCreateLayerByName (targetLayerName);
		if (targetLayerIndex == APIInvalidAttributeIndex) {
			res.Add ("success", false);
			res.Add ("error", "Could not find or create target layer");
			return res;
		}

		UInt32 modifiedCount = 0;
		GSErrCode lastErr = NoError;
		ACAPI_CallUndoableCommand ("Change Elements Layer", [&] () -> GSErrCode {
			for (UIndex i = 0; i < elements.GetSize (); ++i) {
				API_Guid elemGuid = APIGuidFromString (elements[i].ToCStr ());
				API_Element element {};
				element.header.guid = elemGuid;
				if (ACAPI_Element_GetHeader (&element.header) == NoError) {
					if (ACAPI_Element_Get (&element) == NoError) {
						API_Element mask {};
						ACAPI_ELEMENT_MASK_CLEAR (mask);
						ACAPI_ELEMENT_MASK_SET (mask, API_Elem_Head, layer);
						element.header.layer = targetLayerIndex;
						GSErrCode chgErr = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
						if (chgErr == NoError) {
							modifiedCount++;
						} else {
							lastErr = chgErr;
						}
					}
				}
			}
			return NoError;
		});

		res.Add ("success", true);
		res.Add ("modifiedCount", (Int32) modifiedCount);
		res.Add ("layerName", targetLayerName);
		res.Add ("layerIndex", targetLayerIndex.ToInt32_Deprecated ());
		if (modifiedCount == 0 && lastErr != NoError) {
			res.Add ("lastError", (Int32) lastErr);
		}
		return res;
	}
};

// =============================================================================
// 3. ManageHotlinks Command (list, break, delete, import)
// =============================================================================

class ManageHotlinksCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ManageHotlinks";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString action = "list";
		parameters.Get ("action", action);

		if (action == "list") {
			GS::Array<API_Guid> nodeList;
			GSErrCode err = ACAPI_Hotlink_GetHotlinkNodes (nullptr, &nodeList);
			GS::Array<GS::ObjectState> nodesArray;
			if (err == NoError) {
				for (UIndex i = 0; i < nodeList.GetSize (); ++i) {
					API_HotlinkNode node {};
					node.guid = nodeList[i];
					if (ACAPI_Hotlink_GetHotlinkNode (&node) == NoError) {
						GS::ObjectState nState;
						nState.Add ("guid", APIGuidToString (node.guid));
						nState.Add ("name", GS::UniString (node.name));
						if (node.sourceLocation != nullptr) {
							nState.Add ("sourceLocation", node.sourceLocation->ToDisplayText ());
						}
						GS::Array<API_Guid> instances;
						ACAPI_Hotlink_GetHotlinkInstances (&node.guid, &instances);
						nState.Add ("instanceCount", (Int32) instances.GetSize ());
						nodesArray.Push (nState);
					}
				}
			}
			res.Add ("success", true);
			res.Add ("nodes", nodesArray);
			return res;
		}

		if (action == "break") {
			GS::UniString targetGuidStr;
			parameters.Get ("nodeGuid", targetGuidStr);
			GS::Array<API_Guid> toBreak;
			if (!targetGuidStr.IsEmpty ()) {
				toBreak.Push (APIGuidFromString (targetGuidStr.ToCStr ()));
			} else {
				ACAPI_Hotlink_GetHotlinkNodes (nullptr, &toBreak);
			}

			Int32 brokenCount = 0;
			ACAPI_CallUndoableCommand ("Break Hotlinks", [&] () -> GSErrCode {
				for (UIndex i = 0; i < toBreak.GetSize (); ++i) {
					if (ACAPI_Hotlink_BreakHotlinkNode (&(toBreak[i])) == NoError) {
						brokenCount++;
					}
				}
				return NoError;
			});
			res.Add ("success", true);
			res.Add ("brokenCount", brokenCount);
			return res;
		}

		if (action == "delete") {
			GS::UniString targetGuidStr;
			parameters.Get ("nodeGuid", targetGuidStr);
			GS::Array<API_Guid> toDelete;
			if (!targetGuidStr.IsEmpty ()) {
				toDelete.Push (APIGuidFromString (targetGuidStr.ToCStr ()));
			} else {
				ACAPI_Hotlink_GetHotlinkNodes (nullptr, &toDelete);
			}

			Int32 deletedCount = 0;
			ACAPI_CallUndoableCommand ("Delete Hotlinks", [&] () -> GSErrCode {
				for (UIndex i = 0; i < toDelete.GetSize (); ++i) {
					if (ACAPI_Hotlink_DeleteHotlinkNode (&(toDelete[i])) == NoError) {
						deletedCount++;
					}
				}
				return NoError;
			});
			res.Add ("success", true);
			res.Add ("deletedCount", deletedCount);
			return res;
		}

		if (action == "import") {
			GS::UniString filePath;
			if (!parameters.Get ("filePath", filePath) || filePath.IsEmpty ()) {
				res.Add ("success", false);
				res.Add ("error", "filePath parameter is required for action='import'");
				return res;
			}

			GS::UniString nodeName = "SuperquadraHotlink";
			parameters.Get ("name", nodeName);
			GS::UniString layerName;
			parameters.Get ("layerName", layerName);
			double posX = 0.0, posY = 0.0, posZ = 0.0;
			parameters.Get ("x", posX);
			parameters.Get ("y", posY);
			parameters.Get ("z", posZ);
			short floorInd = 0;
			Int32 floorParam = 0;
			if (parameters.Get ("floorInd", floorParam)) {
				floorInd = (short) floorParam;
			}
			bool breakLink = false;
			parameters.Get ("breakLink", breakLink);
			bool singleStory = false;
			parameters.Get ("singleStory", singleStory);

			API_Guid createdNodeGuid = APINULLGuid;
			API_Guid createdElemGuid = APINULLGuid;
			GSErrCode importErr = NoError;

			ACAPI_CallUndoableCommand ("Import Hotlink Module", [&] () -> GSErrCode {
				API_HotlinkNode hotlinkNode {};
				hotlinkNode.type = APIHotlink_Module;
				hotlinkNode.storyRangeType = singleStory ? APIHotlink_SingleStory : APIHotlink_AllStories;
				hotlinkNode.sourceLocation = new IO::Location (filePath);
				GS::ucsncpy (hotlinkNode.name, nodeName.ToUStr (), API_UniLongNameLen - 1);
				hotlinkNode.refFloorInd = floorInd;

				ACAPI_Hotlink_GetHotlinkStoryInfo (&hotlinkNode);
				importErr = ACAPI_Hotlink_CreateHotlinkNode (&hotlinkNode);
				if (importErr != NoError) {
					return importErr;
				}
				createdNodeGuid = hotlinkNode.guid;

				API_Element element {};
				element.header.type = API_HotlinkID;
				element.header.floorInd = floorInd;
				element.header.layer = FindOrCreateLayerByName (layerName);
				if (element.header.layer == APIInvalidAttributeIndex) {
					element.header.layer = APIApplicationLayerAttributeIndex;
				}
				element.hotlink.type = APIHotlink_Module;
				element.hotlink.hotlinkNodeGuid = hotlinkNode.guid;
				element.hotlink.ignoreTopFloorLinks = true;

				for (Int32 j = 0; j < 12; ++j) {
					element.hotlink.transformation.tmx[j] = 0.0;
				}
				element.hotlink.transformation.tmx[0]  = 1.0;
				element.hotlink.transformation.tmx[5]  = 1.0;
				element.hotlink.transformation.tmx[10] = 1.0;
				element.hotlink.transformation.tmx[3]  = posX;
				element.hotlink.transformation.tmx[7]  = posY;
				element.hotlink.transformation.tmx[11] = posZ;

				importErr = ACAPI_Element_Create (&element, nullptr);
				if (importErr == NoError) {
					createdElemGuid = element.header.guid;
					if (breakLink) {
						ACAPI_Hotlink_BreakHotlinkNode (&createdNodeGuid);
					}
				}
				return importErr;
			});

			res.Add ("success", importErr == NoError);
			res.Add ("nodeGuid", APIGuidToString (createdNodeGuid));
			res.Add ("elementGuid", APIGuidToString (createdElemGuid));
			res.Add ("broken", breakLink);
			if (importErr != NoError) {
				res.Add ("errorCode", (Int32) importErr);
			}
			return res;
		}

		res.Add ("success", false);
		res.Add ("error", "Unsupported action: use 'list', 'break', 'delete', or 'import'");
		return res;
	}
};

// =============================================================================
// 4. DeleteElements Command
// =============================================================================

class DeleteElementsCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "DeleteElements";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::Array<GS::UniString> elements;
		if (!parameters.Get ("elements", elements) || elements.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "elements array parameter is required");
			return res;
		}

		GS::Array<API_Guid> guidsToDelete;
		for (UIndex i = 0; i < elements.GetSize (); ++i) {
			guidsToDelete.Push (APIGuidFromString (elements[i].ToCStr ()));
		}

		GSErrCode err = ACAPI_CallUndoableCommand ("Delete Elements", [&] () -> GSErrCode {
			return ACAPI_Element_Delete (guidsToDelete);
		});

		res.Add ("success", err == NoError);
		res.Add ("deletedCount", (Int32) guidsToDelete.GetSize ());
		if (err != NoError) {
			res.Add ("errorCode", (Int32) err);
		}
		return res;
	}
};

// =============================================================================
// 5. ListLayers Command
// =============================================================================

class ListLayersCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ListLayers";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		UInt32 count = 0;
		GSErrCode err = ACAPI_Attribute_GetNum (API_LayerID, count);
		if (err != NoError) {
			res.Add ("success", false);
			res.Add ("error", "Failed to retrieve layer count");
			return res;
		}

		GS::Array<GS::ObjectState> layersArray;
		API_Attribute attr {};
		attr.header.typeID = API_LayerID;

		for (UInt32 i = 1; i <= count; ++i) {
			attr.header.index = ACAPI_CreateAttributeIndex ((Int32) i);
			if (ACAPI_Attribute_Get (&attr) == NoError) {
				GS::ObjectState lState;
				lState.Add ("index", attr.header.index.ToInt32_Deprecated ());
				lState.Add ("name", GS::UniString (attr.header.name));
				lState.Add ("guid", APIGuidToString (attr.header.guid));
				lState.Add ("locked", (attr.layer.head.flags & APILay_Locked) != 0);
				lState.Add ("hidden", (attr.layer.head.flags & APILay_Hidden) != 0);
				layersArray.Push (lState);
			}
		}

		res.Add ("success", true);
		res.Add ("layerCount", (Int32) layersArray.GetSize ());
		res.Add ("layers", layersArray);
		return res;
	}
};

// =============================================================================
// 6. ModifyLayersState Command (lock/unlock, show/hide, delete layers)
// =============================================================================

class ModifyLayersStateCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ModifyLayersState";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::Array<GS::UniString> layerNames;
		parameters.Get ("layers", layerNames);

		bool hasLocked = parameters.Contains ("locked");
		bool lockedVal = false;
		if (hasLocked) {
			parameters.Get ("locked", lockedVal);
		}

		bool hasHidden = parameters.Contains ("hidden");
		bool hiddenVal = false;
		if (hasHidden) {
			parameters.Get ("hidden", hiddenVal);
		}

		bool deleteLayer = false;
		parameters.Get ("deleteLayer", deleteLayer);

		Int32 modifiedCount = 0;
		ACAPI_CallUndoableCommand ("Modify Layers State", [&] () -> GSErrCode {
			for (UIndex i = 0; i < layerNames.GetSize (); ++i) {
				API_Attr_Head attrHead {};
				attrHead.typeID = API_LayerID;
				GS::UniString searchName = layerNames[i];
				attrHead.uniStringNamePtr = &searchName;
				if (ACAPI_Attribute_Search (&attrHead) == NoError && attrHead.index != APIApplicationLayerAttributeIndex) {
					if (deleteLayer) {
						if (ACAPI_Attribute_Delete (attrHead) == NoError) {
							modifiedCount++;
						}
					} else {
						API_Attribute attr {};
						attr.header.typeID = API_LayerID;
						attr.header.index = attrHead.index;
						if (ACAPI_Attribute_Get (&attr) == NoError) {
							if (hasLocked) {
								if (lockedVal) attr.layer.head.flags |= APILay_Locked;
								else attr.layer.head.flags &= ~APILay_Locked;
							}
							if (hasHidden) {
								if (hiddenVal) attr.layer.head.flags |= APILay_Hidden;
								else attr.layer.head.flags &= ~APILay_Hidden;
							}
							if (ACAPI_Attribute_Modify (&attr, nullptr) == NoError) {
								modifiedCount++;
							}
						}
					}
				}
			}
			return NoError;
		});

		res.Add ("success", true);
		res.Add ("modifiedCount", modifiedCount);
		return res;
	}
};

// =============================================================================
// 7. PublishSet Command
// =============================================================================

class PublishSetCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "PublishSet";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString targetSetName;
		parameters.Get ("setName", targetSetName);
		GS::UniString outPathStr;
		parameters.Get ("outputPath", outPathStr);

		Int32 numOfSets = 0;
		GSErrCode err = ACAPI_Navigator_GetNavigatorSetNum (&numOfSets);
		if (err != NoError || numOfSets <= 0) {
			res.Add ("success", false);
			res.Add ("error", "No navigator sets found in project");
			return res;
		}

		API_Guid publisherSetGuid = APINULLGuid;
		GS::UniString foundSetName;

		for (Int32 i = 0; i < numOfSets; ++i) {
			API_NavigatorSet set {};
			set.mapId = API_PublisherSets;
			if (ACAPI_Navigator_GetNavigatorSet (&set, &i) == NoError) {
				GS::UniString sName (set.name);
				if (targetSetName.IsEmpty () || sName.Contains (targetSetName)) {
					publisherSetGuid = set.rootGuid;
					foundSetName = sName;
					break;
				}
			}
		}

		if (publisherSetGuid == APINULLGuid) {
			res.Add ("success", false);
			res.Add ("error", "Requested publisher set was not found");
			return res;
		}

		API_PublishPars publishPars {};
		publishPars.guid = publisherSetGuid;
		IO::Location* customLoc = nullptr;
		if (!outPathStr.IsEmpty ()) {
			customLoc = new IO::Location (outPathStr);
			publishPars.path = customLoc;
		}

		err = ACAPI_ProjectOperation_Publish (&publishPars);
		if (customLoc != nullptr) {
			delete customLoc;
		}

		res.Add ("success", err == NoError);
		res.Add ("publisherSet", foundSetName);
		if (err != NoError) {
			res.Add ("errorCode", (Int32) err);
		}
		return res;
	}
};

// =============================================================================
// 8. ListLayoutDatabases Command
// =============================================================================

class ListLayoutDatabasesCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ListLayoutDatabases";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& /*parameters*/, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::Array<API_DatabaseUnId> layoutDbs;
		GSErrCode err = ACAPI_Database_GetLayoutDatabases (nullptr, &layoutDbs);
		if (err != NoError) {
			res.Add ("success", false);
			res.Add ("error", "Failed to retrieve layout databases");
			return res;
		}

		GS::Array<GS::ObjectState> layoutsArray;
		for (UIndex i = 0; i < layoutDbs.GetSize (); ++i) {
			API_DatabaseInfo dbInfo {};
			dbInfo.databaseUnId = layoutDbs[i];
			dbInfo.typeID = APIWind_LayoutID;
			if (ACAPI_Window_GetDatabaseInfo (&dbInfo) == NoError) {
				GS::ObjectState lState;
				lState.Add ("guid", APIGuidToString (dbInfo.databaseUnId.elemSetId));
				lState.Add ("name", GS::UniString (dbInfo.name));
				lState.Add ("title", GS::UniString (dbInfo.title));
				lState.Add ("ref", GS::UniString (dbInfo.ref));
				layoutsArray.Push (lState);
			}
		}

		res.Add ("success", true);
		res.Add ("count", (Int32) layoutsArray.GetSize ());
		res.Add ("layouts", layoutsArray);
		return res;
	}
};

// =============================================================================
// 9. PlaceDrawingOnLayout Command (single & batch layout sheet automation)
// =============================================================================

class PlaceDrawingOnLayoutCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "PlaceDrawingOnLayout";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;

		struct PlacementSpec {
			API_Guid layoutGuid;
			API_Guid viewGuid;
			double posX;
			double posY;
			double ratio;
		};

		GS::Array<PlacementSpec> specs;
		GS::Array<GS::ObjectState> placementsParam;
		if (parameters.Get ("placements", placementsParam) && !placementsParam.IsEmpty ()) {
			for (UIndex i = 0; i < placementsParam.GetSize (); ++i) {
				GS::UniString lStr, vStr;
				placementsParam[i].Get ("layoutGuid", lStr);
				placementsParam[i].Get ("viewGuid", vStr);
				double px = 0.21, py = 0.1485, rat = 1.0;
				placementsParam[i].Get ("posX", px);
				placementsParam[i].Get ("posY", py);
				placementsParam[i].Get ("ratio", rat);
				if (!lStr.IsEmpty () && !vStr.IsEmpty ()) {
					specs.Push ({ APIGuidFromString (lStr.ToCStr ()), APIGuidFromString (vStr.ToCStr ()), px, py, rat });
				}
			}
		} else {
			GS::UniString lStr, vStr;
			parameters.Get ("layoutGuid", lStr);
			parameters.Get ("viewGuid", vStr);
			double px = 0.21, py = 0.1485, rat = 1.0;
			parameters.Get ("posX", px);
			parameters.Get ("posY", py);
			parameters.Get ("ratio", rat);
			if (!lStr.IsEmpty () && !vStr.IsEmpty ()) {
				specs.Push ({ APIGuidFromString (lStr.ToCStr ()), APIGuidFromString (vStr.ToCStr ()), px, py, rat });
			}
		}

		if (specs.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "Provide layoutGuid and viewGuid, or placements array");
			return res;
		}

		API_DatabaseInfo origDb {};
		ACAPI_Database_GetCurrentDatabase (&origDb);

		GS::Array<GS::UniString> createdDrawings;
		GSErrCode lastErr = NoError;

		ACAPI_CallUndoableCommand ("Place Drawings On Layout", [&] () -> GSErrCode {
			for (UIndex i = 0; i < specs.GetSize (); ++i) {
				API_DatabaseInfo targetDb {};
				targetDb.typeID = APIWind_LayoutID;
				targetDb.databaseUnId.elemSetId = specs[i].layoutGuid;

				GSErrCode dbErr = ACAPI_Database_ChangeCurrentDatabase (&targetDb);
				if (dbErr != NoError) {
					lastErr = dbErr;
					continue;
				}

				API_Element element {};
				element.header.type = API_DrawingID;
				if (ACAPI_Element_GetDefaults (&element, nullptr) == NoError) {
					element.drawing.drawingGuid = specs[i].viewGuid;
					element.drawing.pos.x = specs[i].posX;
					element.drawing.pos.y = specs[i].posY;
					element.drawing.ratio = specs[i].ratio;
					element.drawing.anchorPoint = APIAnc_MM;
					element.drawing.isCutWithFrame = false;

					GSErrCode crErr = ACAPI_Element_Create (&element, nullptr);
					if (crErr == NoError) {
						createdDrawings.Push (APIGuidToString (element.header.guid));
					} else {
						lastErr = crErr;
					}
				}
			}

			if (origDb.typeID != API_ZombieWindowID) {
				ACAPI_Database_ChangeCurrentDatabase (&origDb);
			}
			return NoError;
		});

		res.Add ("success", !createdDrawings.IsEmpty ());
		res.Add ("placedCount", (Int32) createdDrawings.GetSize ());
		res.Add ("drawingGuids", createdDrawings);
		if (lastErr != NoError) {
			res.Add ("lastError", (Int32) lastErr);
		}
		return res;
	}
};

// =============================================================================
// 10. SetFloorPlanCutPlane Command
// =============================================================================

class SetFloorPlanCutPlaneCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "SetFloorPlanCutPlane";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		double cutLevel = 3.20;
		parameters.Get ("cutLevel", cutLevel);

		API_FloorPlanCutDefinition cutDef {};
		GSErrCode err = ACAPI_ProjectSetting_GetPreferences (&cutDef, APIPrefs_FloorPlanCutPlaneDef);
		if (err == NoError) {
			cutDef.currCutLevel = cutLevel;
			if (parameters.Contains ("topCutLevel")) {
				parameters.Get ("topCutLevel", cutDef.topCutLevel);
			}
			if (parameters.Contains ("bottomCutLevel")) {
				parameters.Get ("bottomCutLevel", cutDef.bottomCutLevel);
			}
			err = ACAPI_ProjectSetting_SetPreferences (&cutDef, APIPrefs_FloorPlanCutPlaneDef);
		}

		res.Add ("success", err == NoError);
		res.Add ("cutLevel", cutLevel);
		if (err != NoError) {
			res.Add ("errorCode", (Int32) err);
		}
		return res;
	}
};

// =============================================================================
// 11. SolidElementOperations Command (SEO Boolean Link / Unlink)
// =============================================================================

class SolidElementOperationsCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "SolidElementOperations";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString action = "link";
		parameters.Get ("action", action);

		GS::Array<GS::UniString> targets;
		GS::Array<GS::UniString> operators;
		parameters.Get ("targets", targets);
		parameters.Get ("operators", operators);

		if (targets.IsEmpty () || operators.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "targets and operators GUID arrays are required");
			return res;
		}

		GS::UniString opStr = "substract_upward";
		parameters.Get ("operation", opStr);

		API_SolidOperationID solidOp = APISolid_SubstUp;
		if (opStr == "substract") solidOp = APISolid_Substract;
		else if (opStr == "substract_upward") solidOp = APISolid_SubstUp;
		else if (opStr == "substract_downward") solidOp = APISolid_SubstDown;
		else if (opStr == "intersect") solidOp = APISolid_Intersect;
		else if (opStr == "add") solidOp = APISolid_Add;

		Int32 linkedCount = 0;
		GSErrCode lastErr = NoError;

		ACAPI_CallUndoableCommand ("Solid Element Operations", [&] () -> GSErrCode {
			for (UIndex t = 0; t < targets.GetSize (); ++t) {
				API_Guid targetGuid = APIGuidFromString (targets[t].ToCStr ());
				for (UIndex o = 0; o < operators.GetSize (); ++o) {
					API_Guid operatorGuid = APIGuidFromString (operators[o].ToCStr ());
					GSErrCode seoErr = NoError;
					if (action == "unlink") {
						seoErr = ACAPI_Element_SolidLink_Remove (targetGuid, operatorGuid);
					} else {
						seoErr = ACAPI_Element_SolidLink_Create (targetGuid, operatorGuid, solidOp, APISolidFlag_OperatorAttrib);
					}
					if (seoErr == NoError) {
						linkedCount++;
					} else {
						lastErr = seoErr;
					}
				}
			}
			return NoError;
		});

		res.Add ("success", linkedCount > 0);
		res.Add ("linkedCount", linkedCount);
		res.Add ("operation", opStr);
		if (lastErr != NoError) {
			res.Add ("lastError", (Int32) lastErr);
		}
		return res;
	}
};

// =============================================================================
// 12. ModifyElementsGeometry Command (Transform / Edit / Set Coordinates & ID)
// =============================================================================

class ModifyElementsGeometryCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "ModifyElementsGeometry";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;

		// Option A: Batch transform via ACAPI_Element_Edit (drag, elevate, rotate)
		GS::UniString editAction;
		if (parameters.Get ("editAction", editAction) && !editAction.IsEmpty ()) {
			GS::Array<GS::UniString> elemGuids;
			parameters.Get ("elements", elemGuids);
			if (elemGuids.IsEmpty ()) {
				res.Add ("success", false);
				res.Add ("error", "elements array is required for editAction");
				return res;
			}

			GS::Array<API_Neig> items;
			for (UIndex i = 0; i < elemGuids.GetSize (); ++i) {
				items.Push (API_Neig (APIGuidFromString (elemGuids[i].ToCStr ())));
			}

			double dx = 0.0, dy = 0.0, dz = 0.0;
			parameters.Get ("dx", dx);
			parameters.Get ("dy", dy);
			parameters.Get ("dz", dz);
			bool copyElements = false;
			parameters.Get ("copy", copyElements);

			API_EditPars editPars {};
			editPars.withDelete = !copyElements;
			editPars.begC = { 0.0, 0.0, 0.0 };
			editPars.endC = { dx, dy, dz };

			if (editAction == "elevate") {
				editPars.typeID = APIEdit_Elevate;
			} else if (editAction == "rotate") {
				editPars.typeID = APIEdit_Rotate;
				double ox = 0.0, oy = 0.0, angleDeg = 90.0;
				parameters.Get ("originX", ox);
				parameters.Get ("originY", oy);
				parameters.Get ("angleDeg", angleDeg);
				double rad = angleDeg * 3.14159265358979323846 / 180.0;
				editPars.origC = { ox, oy };
				editPars.begC = { ox + 1.0, oy, 0.0 };
				editPars.endC = { ox + std::cos (rad), oy + std::sin (rad), dz };
			} else {
				editPars.typeID = APIEdit_Drag;
			}

			GSErrCode editErr = ACAPI_CallUndoableCommand ("Transform Elements", [&] () -> GSErrCode {
				return ACAPI_Element_Edit (&items, editPars);
			});

			res.Add ("success", editErr == NoError);
			res.Add ("editedCount", (Int32) items.GetSize ());
			if (editErr != NoError) {
				res.Add ("errorCode", (Int32) editErr);
			}
			return res;
		}

		// Option B: Per-element direct parametric modification
		GS::Array<GS::ObjectState> modifications;
		if (!parameters.Get ("modifications", modifications) || modifications.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "Provide either editAction or modifications array");
			return res;
		}

		Int32 modifiedCount = 0;
		GSErrCode lastErr = NoError;

		ACAPI_CallUndoableCommand ("Modify Elements Geometry", [&] () -> GSErrCode {
			for (UIndex i = 0; i < modifications.GetSize (); ++i) {
				const GS::ObjectState& mod = modifications[i];
				GS::UniString guidStr;
				if (!mod.Get ("guid", guidStr) || guidStr.IsEmpty ()) {
					continue;
				}
				API_Guid elemGuid = APIGuidFromString (guidStr.ToCStr ());

				// Optional Element ID rename
				GS::UniString newElemId;
				if (mod.Get ("elemId", newElemId) && !newElemId.IsEmpty ()) {
					ACAPI_Element_ChangeElementInfoString (&elemGuid, &newElemId);
				}

				API_Element element {};
				element.header.guid = elemGuid;
				if (ACAPI_Element_Get (&element) != NoError) {
					continue;
				}

				API_Element mask {};
				ACAPI_ELEMENT_MASK_CLEAR (mask);
				bool hasChanges = false;

				Int32 floorInd = 0;
				if (mod.Get ("floorInd", floorInd)) {
					element.header.floorInd = (short) floorInd;
					ACAPI_ELEMENT_MASK_SET (mask, API_Elem_Head, floorInd);
					hasChanges = true;
				}

				GS::UniString layerName;
				if (mod.Get ("layerName", layerName) && !layerName.IsEmpty ()) {
					API_AttributeIndex lIdx = FindOrCreateLayerByName (layerName);
					if (lIdx != APIInvalidAttributeIndex) {
						element.header.layer = lIdx;
						ACAPI_ELEMENT_MASK_SET (mask, API_Elem_Head, layer);
						hasChanges = true;
					}
				}

				if (element.header.type == API_WallID) {
					if (mod.Contains ("begX") && mod.Contains ("begY")) {
						mod.Get ("begX", element.wall.begC.x);
						mod.Get ("begY", element.wall.begC.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, begC);
						hasChanges = true;
					}
					if (mod.Contains ("endX") && mod.Contains ("endY")) {
						mod.Get ("endX", element.wall.endC.x);
						mod.Get ("endY", element.wall.endC.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, endC);
						hasChanges = true;
					}
					if (mod.Contains ("height")) {
						mod.Get ("height", element.wall.height);
						element.wall.relativeTopStory = 0;
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, height);
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, relativeTopStory);
						hasChanges = true;
					}
					if (mod.Contains ("bottomOffset")) {
						mod.Get ("bottomOffset", element.wall.bottomOffset);
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, bottomOffset);
						hasChanges = true;
					}
					if (mod.Contains ("thickness")) {
						mod.Get ("thickness", element.wall.thickness);
						ACAPI_ELEMENT_MASK_SET (mask, API_WallType, thickness);
						hasChanges = true;
					}
				} else if (element.header.type == API_ColumnID) {
					if (mod.Contains ("posX") && mod.Contains ("posY")) {
						mod.Get ("posX", element.column.origoPos.x);
						mod.Get ("posY", element.column.origoPos.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_ColumnType, origoPos);
						hasChanges = true;
					}
					if (mod.Contains ("bottomOffset")) {
						mod.Get ("bottomOffset", element.column.bottomOffset);
						ACAPI_ELEMENT_MASK_SET (mask, API_ColumnType, bottomOffset);
						hasChanges = true;
					}
					if (mod.Contains ("height")) {
						mod.Get ("height", element.column.height);
						element.column.relativeTopStory = 0;
						ACAPI_ELEMENT_MASK_SET (mask, API_ColumnType, height);
						ACAPI_ELEMENT_MASK_SET (mask, API_ColumnType, relativeTopStory);
						hasChanges = true;
					}
				} else if (element.header.type == API_BeamID) {
					if (mod.Contains ("begX") && mod.Contains ("begY")) {
						mod.Get ("begX", element.beam.begC.x);
						mod.Get ("begY", element.beam.begC.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_BeamType, begC);
						hasChanges = true;
					}
					if (mod.Contains ("endX") && mod.Contains ("endY")) {
						mod.Get ("endX", element.beam.endC.x);
						mod.Get ("endY", element.beam.endC.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_BeamType, endC);
						hasChanges = true;
					}
					if (mod.Contains ("level")) {
						mod.Get ("level", element.beam.level);
						ACAPI_ELEMENT_MASK_SET (mask, API_BeamType, level);
						hasChanges = true;
					}
					if (mod.Contains ("offset")) {
						mod.Get ("offset", element.beam.offset);
						ACAPI_ELEMENT_MASK_SET (mask, API_BeamType, offset);
						hasChanges = true;
					}
				} else if (element.header.type == API_SlabID) {
					if (mod.Contains ("level")) {
						mod.Get ("level", element.slab.level);
						ACAPI_ELEMENT_MASK_SET (mask, API_SlabType, level);
						hasChanges = true;
					}
					if (mod.Contains ("thickness")) {
						mod.Get ("thickness", element.slab.thickness);
						ACAPI_ELEMENT_MASK_SET (mask, API_SlabType, thickness);
						hasChanges = true;
					}
				} else if (element.header.type == API_ObjectID) {
					if (mod.Contains ("posX") && mod.Contains ("posY")) {
						mod.Get ("posX", element.object.pos.x);
						mod.Get ("posY", element.object.pos.y);
						ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, pos);
						hasChanges = true;
					}
					if (mod.Contains ("level")) {
						mod.Get ("level", element.object.level);
						ACAPI_ELEMENT_MASK_SET (mask, API_ObjectType, level);
						hasChanges = true;
					}
				}

				if (hasChanges) {
					GSErrCode chgErr = ACAPI_Element_Change (&element, &mask, nullptr, 0, true);
					if (chgErr == NoError) {
						modifiedCount++;
					} else {
						lastErr = chgErr;
					}
				} else if (!newElemId.IsEmpty ()) {
					modifiedCount++;
				}
			}
			return NoError;
		});

		res.Add ("success", modifiedCount > 0);
		res.Add ("modifiedCount", modifiedCount);
		if (lastErr != NoError) {
			res.Add ("lastError", (Int32) lastErr);
		}
		return res;
	}
};

// =============================================================================
// 13. CreateNativeElements Command (Batch create Wall, Column, Beam, Window, Door)
// =============================================================================

class CreateNativeElementsCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "CreateNativeElements";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::Array<GS::ObjectState> items;
		if (!parameters.Get ("elements", items) || items.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "elements array parameter is required");
			return res;
		}

		GS::Array<GS::UniString> createdGuids;
		GSErrCode lastErr = NoError;

		ACAPI_CallUndoableCommand ("Create Native Elements", [&] () -> GSErrCode {
			for (UIndex i = 0; i < items.GetSize (); ++i) {
				const GS::ObjectState& spec = items[i];
				GS::UniString typeStr = "Wall";
				spec.Get ("type", typeStr);

				API_Element element {};
				API_ElementMemo memo {};
				bool hasMemo = false;

				if (typeStr == "Wall") element.header.type = API_WallID;
				else if (typeStr == "Column") element.header.type = API_ColumnID;
				else if (typeStr == "Beam") element.header.type = API_BeamID;
				else if (typeStr == "Door") element.header.type = API_DoorID;
				else if (typeStr == "Window") element.header.type = API_WindowID;
				else if (typeStr == "Object") element.header.type = API_ObjectID;
				else continue;

				if (ACAPI_Element_GetDefaults (&element, &memo) != NoError) {
					ACAPI_DisposeElemMemoHdls (&memo);
					continue;
				}
				hasMemo = true;

				Int32 floorInd = 0;
				if (spec.Get ("floorInd", floorInd)) {
					element.header.floorInd = (short) floorInd;
				}

				GS::UniString layerName;
				if (spec.Get ("layerName", layerName) && !layerName.IsEmpty ()) {
					API_AttributeIndex lIdx = FindOrCreateLayerByName (layerName);
					if (lIdx != APIInvalidAttributeIndex) {
						element.header.layer = lIdx;
					}
				}

				if (typeStr == "Wall") {
					spec.Get ("begX", element.wall.begC.x);
					spec.Get ("begY", element.wall.begC.y);
					spec.Get ("endX", element.wall.endC.x);
					spec.Get ("endY", element.wall.endC.y);
					if (spec.Contains ("height")) {
						spec.Get ("height", element.wall.height);
						element.wall.relativeTopStory = 0;
					}
					if (spec.Contains ("bottomOffset")) {
						spec.Get ("bottomOffset", element.wall.bottomOffset);
					}
					if (spec.Contains ("thickness")) {
						spec.Get ("thickness", element.wall.thickness);
						element.wall.modelElemStructureType = API_BasicStructure;
					}
				} else if (typeStr == "Column") {
					spec.Get ("posX", element.column.origoPos.x);
					spec.Get ("posY", element.column.origoPos.y);
					if (spec.Contains ("bottomOffset")) {
						spec.Get ("bottomOffset", element.column.bottomOffset);
					}
					if (spec.Contains ("height")) {
						spec.Get ("height", element.column.height);
						element.column.relativeTopStory = 0;
					}
				} else if (typeStr == "Beam") {
					spec.Get ("begX", element.beam.begC.x);
					spec.Get ("begY", element.beam.begC.y);
					spec.Get ("endX", element.beam.endC.x);
					spec.Get ("endY", element.beam.endC.y);
					if (spec.Contains ("level")) {
						spec.Get ("level", element.beam.level);
					}
				} else if (typeStr == "Door" || typeStr == "Window") {
					GS::UniString wallGuidStr;
					spec.Get ("wallGuid", wallGuidStr);
					double objLoc = 0.5, lower = 0.0, width = 0.90, height = 2.10;
					spec.Get ("objLoc", objLoc);
					spec.Get ("lower", lower);
					spec.Get ("width", width);
					spec.Get ("height", height);
					if (typeStr == "Door") {
						element.door.owner = APIGuidFromString (wallGuidStr.ToCStr ());
						element.door.objLoc = objLoc;
						element.door.lower = lower;
						element.door.openingBase.width = width;
						element.door.openingBase.height = height;
					} else {
						element.window.owner = APIGuidFromString (wallGuidStr.ToCStr ());
						element.window.objLoc = objLoc;
						element.window.lower = lower;
						element.window.openingBase.width = width;
						element.window.openingBase.height = height;
					}
				} else if (typeStr == "Object") {
					spec.Get ("posX", element.object.pos.x);
					spec.Get ("posY", element.object.pos.y);
					if (spec.Contains ("level")) {
						spec.Get ("level", element.object.level);
					}
				}

				GSErrCode crErr = ACAPI_Element_Create (&element, hasMemo ? &memo : nullptr);
				if (hasMemo) {
					ACAPI_DisposeElemMemoHdls (&memo);
				}

				if (crErr == NoError) {
					createdGuids.Push (APIGuidToString (element.header.guid));
					GS::UniString elemId;
					if (spec.Get ("elemId", elemId) && !elemId.IsEmpty ()) {
						ACAPI_Element_ChangeElementInfoString (&element.header.guid, &elemId);
					}
				} else {
					lastErr = crErr;
				}
			}
			return NoError;
		});

		res.Add ("success", !createdGuids.IsEmpty ());
		res.Add ("createdCount", (Int32) createdGuids.GetSize ());
		res.Add ("createdGuids", createdGuids);
		if (lastErr != NoError) {
			res.Add ("lastError", (Int32) lastErr);
		}
		return res;
	}
};

// =============================================================================
// 14. GetSetGDLParameters Command (Inspect & Modify GDL AddPars in Memo)
// =============================================================================

class GetSetGDLParametersCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "GetSetGDLParameters";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString guidStr;
		if (!parameters.Get ("guid", guidStr) || guidStr.IsEmpty ()) {
			res.Add ("success", false);
			res.Add ("error", "guid parameter is required");
			return res;
		}

		API_Guid elemGuid = APIGuidFromString (guidStr.ToCStr ());
		GS::UniString action = "get";
		parameters.Get ("action", action);

		API_ElementMemo memo {};
		GSErrCode err = ACAPI_Element_GetMemo (elemGuid, &memo, APIMemoMask_AddPars);
		if (err != NoError || memo.params == nullptr) {
			ACAPI_DisposeElemMemoHdls (&memo);
			res.Add ("success", false);
			res.Add ("error", "Element has no GDL parameters or could not read memo");
			return res;
		}

		GSSize numParams = BMGetHandleSize ((GSHandle) memo.params) / sizeof (API_AddParType);

		if (action == "get") {
			GS::Array<GS::ObjectState> paramList;
			for (GSSize i = 0; i < numParams; ++i) {
				const API_AddParType& par = (*memo.params)[i];
				if (par.typeID == API_ZombieParT || par.typeID == APIParT_Separator) {
					continue;
				}
				GS::ObjectState pState;
				pState.Add ("name", GS::UniString (par.name));
				pState.Add ("typeID", (Int32) par.typeID);
				if (par.typeID == APIParT_CString || par.typeID == APIParT_Title) {
					pState.Add ("valueStr", GS::UniString (par.value.uStr));
				} else {
					pState.Add ("valueNum", par.value.real);
				}
				paramList.Push (pState);
			}
			ACAPI_DisposeElemMemoHdls (&memo);
			res.Add ("success", true);
			res.Add ("count", (Int32) paramList.GetSize ());
			res.Add ("parameters", paramList);
			return res;
		}

		if (action == "set") {
			GS::Array<GS::ObjectState> updates;
			if (!parameters.Get ("updates", updates) || updates.IsEmpty ()) {
				ACAPI_DisposeElemMemoHdls (&memo);
				res.Add ("success", false);
				res.Add ("error", "updates array is required for action='set'");
				return res;
			}

			Int32 updatedCount = 0;
			for (UIndex u = 0; u < updates.GetSize (); ++u) {
				GS::UniString targetName;
				if (!updates[u].Get ("name", targetName) || targetName.IsEmpty ()) {
					continue;
				}
				for (GSSize i = 0; i < numParams; ++i) {
					API_AddParType& par = (*memo.params)[i];
					if (GS::UniString (par.name) == targetName) {
						if (par.typeID == APIParT_CString || par.typeID == APIParT_Title) {
							GS::UniString newStr;
							if (updates[u].Get ("valueStr", newStr)) {
								GS::ucsncpy (par.value.uStr, newStr.ToUStr (), API_UAddParStrLen - 1);
								updatedCount++;
							}
						} else {
							double newNum = 0.0;
							if (updates[u].Get ("valueNum", newNum)) {
								par.value.real = newNum;
								updatedCount++;
							}
						}
						break;
					}
				}
			}

			GSErrCode chgErr = ACAPI_CallUndoableCommand ("Modify GDL Parameters", [&] () -> GSErrCode {
				return ACAPI_Element_ChangeMemo (elemGuid, APIMemoMask_AddPars, &memo);
			});

			ACAPI_DisposeElemMemoHdls (&memo);
			res.Add ("success", chgErr == NoError);
			res.Add ("updatedCount", updatedCount);
			if (chgErr != NoError) {
				res.Add ("errorCode", (Int32) chgErr);
			}
			return res;
		}

		ACAPI_DisposeElemMemoHdls (&memo);
		res.Add ("success", false);
		res.Add ("error", "Unsupported action: use 'get' or 'set'");
		return res;
	}
};

// =============================================================================
// 15. DatabaseAndViewControl Command (rebuild, redraw, save)
// =============================================================================

class DatabaseAndViewControlCommand : public SuperquadraBaseCommand {
public:
	virtual GS::String GetName () const override
	{
		return "DatabaseAndViewControl";
	}

	virtual GS::ObjectState Execute (const GS::ObjectState& parameters, GS::ProcessControl& /*processControl*/) const override
	{
		GS::ObjectState res;
		GS::UniString action = "rebuild";
		parameters.Get ("action", action);

		GSErrCode err = NoError;
		if (action == "rebuild") {
			bool regenerate = true;
			parameters.Get ("regenerate", regenerate);
			err = ACAPI_View_Rebuild (&regenerate);
		} else if (action == "redraw") {
			err = ACAPI_View_Redraw ();
		} else if (action == "save") {
			err = ACAPI_ProjectOperation_Save ();
		} else {
			res.Add ("success", false);
			res.Add ("error", "Unsupported action: use 'rebuild', 'redraw', or 'save'");
			return res;
		}

		res.Add ("success", err == NoError);
		res.Add ("action", action);
		if (err != NoError) {
			res.Add ("errorCode", (Int32) err);
		}
		return res;
	}
};

// =============================================================================
// Dialog & Menu Handling
// =============================================================================

class ExampleDialog : public DG::ModalDialog,
					  public DG::PanelObserver,
					  public DG::ButtonItemObserver,
					  public DG::CompoundItemObserver
{
public:
	enum DialogResourceIds {
		ExampleDialogResourceId = ID_ADDON_DLG,
		OKButtonId = 1,
		CancelButtonId = 2,
		SeparatorId = 3
	};

	ExampleDialog () :
		DG::ModalDialog (ACAPI_GetOwnResModule (), ExampleDialogResourceId, ACAPI_GetOwnResModule ()),
		okButton (GetReference (), OKButtonId),
		cancelButton (GetReference (), CancelButtonId),
		separator (GetReference (), SeparatorId)
	{
		SetTitle ("Superquadra Automation Kernel v2.0.0");
		AttachToAllItems (*this);
		Attach (*this);
	}

	~ExampleDialog ()
	{
		Detach (*this);
		DetachFromAllItems (*this);
	}

private:
	virtual void PanelResized (const DG::PanelResizeEvent& ev) override
	{
		BeginMoveResizeItems ();
		okButton.Move (ev.GetHorizontalChange (), ev.GetVerticalChange ());
		cancelButton.Move (ev.GetHorizontalChange (), ev.GetVerticalChange ());
		separator.MoveAndResize (0, ev.GetVerticalChange (), ev.GetHorizontalChange (), 0);
		EndMoveResizeItems ();
	}

	virtual void ButtonClicked (const DG::ButtonClickEvent& ev) override
	{
		if (ev.GetSource () == &okButton) {
			PostCloseRequest (DG::ModalDialog::Accept);
		} else if (ev.GetSource () == &cancelButton) {
			PostCloseRequest (DG::ModalDialog::Cancel);
		}
	}

	DG::Button    okButton;
	DG::Button    cancelButton;
	DG::Separator separator;
};

static GSErrCode MenuCommandHandler (const API_MenuParams *menuParams)
{
	switch (menuParams->menuItemRef.menuResID) {
		case AddOnMenuID:
			switch (menuParams->menuItemRef.itemIndex) {
				case AddOnCommandID:
					{
						ExampleDialog dialog;
						dialog.Invoke ();
					}
					break;
			}
			break;
	}
	return NoError;
}

// =============================================================================
// Entry Points
// =============================================================================

API_AddonType CheckEnvironment (API_EnvirParams* envir)
{
	RSGetIndString (&envir->addOnInfo.name, AddOnInfoID, AddOnNameID, ACAPI_GetOwnResModule ());
	RSGetIndString (&envir->addOnInfo.description, AddOnInfoID, AddOnDescriptionID, ACAPI_GetOwnResModule ());

	return APIAddon_Preload;
}

GSErrCode RegisterInterface (void)
{
	return ACAPI_MenuItem_RegisterMenu (AddOnMenuID, 0, MenuCode_Tools, MenuFlag_Default);
}

GSErrCode Initialize (void)
{
	ACAPI_MenuItem_InstallMenuHandler (AddOnMenuID, MenuCommandHandler);

	// Register Superquadra Automation Kernel v2.0.0 commands (15 commands)
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<PingCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ChangeElementsLayerCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ManageHotlinksCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<DeleteElementsCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ListLayersCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ModifyLayersStateCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<PublishSetCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ListLayoutDatabasesCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<PlaceDrawingOnLayoutCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<SetFloorPlanCutPlaneCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<SolidElementOperationsCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<ModifyElementsGeometryCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<CreateNativeElementsCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<GetSetGDLParametersCommand> ());
	ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler (GS::NewOwned<DatabaseAndViewControlCommand> ());

	return NoError;
}

GSErrCode FreeData (void)
{
	return NoError;
}
