"""
Superquadra Native Archicad 29 C++ Kernel SDK (v2.0.0)
======================================================
Python wrapper for the `SuperquadraAutomation.apx` native C++ Add-On.
Exposes 15 kernel-level commands under the `"Superquadra"` namespace via
`ACAPI_AddOnAddOnCommunication_InstallAddOnCommandHandler`.
"""

from typing import Any, Dict, List, Optional, Union
from archicad import ACConnection

NAMESPACE = "Superquadra"
SDK_VERSION = "2.0.0"


def _get_connection(conn: Optional[ACConnection] = None) -> ACConnection:
    if conn is None:
        conn = ACConnection.connect()
        if not conn:
            raise RuntimeError("Could not connect to Archicad. Ensure Archicad 29 is open.")
    return conn


def _exec(
    command_name: str,
    params: Optional[Dict[str, Any]] = None,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    conn = _get_connection(conn)
    cmd_id = conn.types.AddOnCommandId(NAMESPACE, command_name)
    return conn.commands.ExecuteAddOnCommand(cmd_id, params or {})


# =============================================================================
# 1. Ping & Diagnostics
# =============================================================================

def ping(conn: Optional[ACConnection] = None) -> Dict[str, Any]:
    """Verify connection to the Superquadra C++ Kernel v2.0.0 and list registered commands."""
    return _exec("Ping", {}, conn)


# =============================================================================
# 2. Layer Management (List, Move Elements, Lock/Unlock, Show/Hide, Delete)
# =============================================================================

def list_layers(conn: Optional[ACConnection] = None) -> Dict[str, Any]:
    """List all layers in the project with index, name, GUID, locked, and hidden states."""
    return _exec("ListLayers", {}, conn)


def change_elements_layer(
    element_guids: List[Union[str, Any]],
    layer_name: str,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """Move elements to the specified layer (automatically creates the layer if missing)."""
    guids_str = [str(g.guid) if hasattr(g, "guid") else str(g) for g in element_guids]
    return _exec(
        "ChangeElementsLayer",
        {"layerName": layer_name, "elements": guids_str},
        conn,
    )


def modify_layers_state(
    layer_names: List[str],
    locked: Optional[bool] = None,
    hidden: Optional[bool] = None,
    delete_layer: bool = False,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """Batch lock/unlock, show/hide, or delete layers in the Archicad kernel."""
    params: Dict[str, Any] = {"layers": layer_names, "deleteLayer": delete_layer}
    if locked is not None:
        params["locked"] = locked
    if hidden is not None:
        params["hidden"] = hidden
    return _exec("ModifyLayersState", params, conn)


# =============================================================================
# 3. Hotlink & Module Lifecycle (List, Import, Break, Delete)
# =============================================================================

def list_hotlinks(conn: Optional[ACConnection] = None) -> Dict[str, Any]:
    """List all Hotlink nodes in the current Archicad project."""
    return _exec("ManageHotlinks", {"action": "list"}, conn)


def import_hotlink_module(
    file_path: str,
    name: str = "SuperquadraHotlink",
    layer_name: str = "",
    x: float = 0.0,
    y: float = 0.0,
    z: float = 0.0,
    floor_ind: int = 0,
    break_link: bool = False,
    single_story: bool = False,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """
    Create a Hotlink Node from an external file (.mod, .pln, .ifc), place an instance
    at (x, y, z) on `floor_ind`, and optionally break the link immediately to convert
    all imported geometries into native Archicad elements.
    """
    return _exec(
        "ManageHotlinks",
        {
            "action": "import",
            "filePath": file_path,
            "name": name,
            "layerName": layer_name,
            "x": float(x),
            "y": float(y),
            "z": float(z),
            "floorInd": int(floor_ind),
            "breakLink": bool(break_link),
            "singleStory": bool(single_story),
        },
        conn,
    )


def break_hotlinks(
    node_guid: Optional[str] = None, conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Break a specific hotlink node (or all hotlinks if node_guid is None) into native elements."""
    params: Dict[str, Any] = {"action": "break"}
    if node_guid:
        params["nodeGuid"] = str(node_guid)
    return _exec("ManageHotlinks", params, conn)


def delete_hotlinks(
    node_guid: Optional[str] = None, conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Delete a specific hotlink node (or all hotlinks if node_guid is None)."""
    params: Dict[str, Any] = {"action": "delete"}
    if node_guid:
        params["nodeGuid"] = str(node_guid)
    return _exec("ManageHotlinks", params, conn)


# =============================================================================
# 4. Element Deletion, Transformation & Parametric Geometry Modification
# =============================================================================

def delete_elements(
    element_guids: List[Union[str, Any]], conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Delete elements by GUID in a single undoable kernel transaction."""
    guids_str = [str(g.guid) if hasattr(g, "guid") else str(g) for g in element_guids]
    return _exec("DeleteElements", {"elements": guids_str}, conn)


def transform_elements(
    element_guids: List[Union[str, Any]],
    edit_action: str = "drag",
    dx: float = 0.0,
    dy: float = 0.0,
    dz: float = 0.0,
    origin_x: float = 0.0,
    origin_y: float = 0.0,
    angle_deg: float = 0.0,
    copy: bool = False,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """
    Transform elements using ACAPI_Element_Edit (`drag`, `elevate`, `rotate`).
    Automatically preserves groups, associative dimensions, and wall openings.
    """
    guids_str = [str(g.guid) if hasattr(g, "guid") else str(g) for g in element_guids]
    return _exec(
        "ModifyElementsGeometry",
        {
            "editAction": edit_action,
            "elements": guids_str,
            "dx": float(dx),
            "dy": float(dy),
            "dz": float(dz),
            "originX": float(origin_x),
            "originY": float(origin_y),
            "angleDeg": float(angle_deg),
            "copy": bool(copy),
        },
        conn,
    )


def modify_elements_geometry(
    modifications: List[Dict[str, Any]], conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """
    Modify element coordinates, heights, offsets, thicknesses, story index, layer, or Element ID.
    Each dict in `modifications` requires `"guid"` and any of:
    `"elemId"`, `"floorInd"`, `"layerName"`, `"begX"`, `"begY"`, `"endX"`, `"endY"`,
    `"posX"`, `"posY"`, `"height"`, `"bottomOffset"`, `"thickness"`, `"level"`, `"offset"`.
    """
    return _exec("ModifyElementsGeometry", {"modifications": modifications}, conn)


# =============================================================================
# 5. Solid Element Operations (SEO Boolean Link / Unlink)
# =============================================================================

def solid_element_operations(
    targets: List[Union[str, Any]],
    operators: List[Union[str, Any]],
    operation: str = "substract_upward",
    action: str = "link",
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """
    Create or remove native Solid Element Operations (SEO) boolean links between targets and operators.
    `operation`: `'substract'`, `'substract_upward'`, `'substract_downward'`, `'intersect'`, or `'add'`.
    `action`: `'link'` or `'unlink'`.
    """
    t_str = [str(g.guid) if hasattr(g, "guid") else str(g) for g in targets]
    o_str = [str(g.guid) if hasattr(g, "guid") else str(g) for g in operators]
    return _exec(
        "SolidElementOperations",
        {
            "action": action,
            "operation": operation,
            "targets": t_str,
            "operators": o_str,
        },
        conn,
    )


# =============================================================================
# 6. Native Parametric Element Creation (Wall, Column, Beam, Door, Window, Object)
# =============================================================================

def create_native_elements(
    elements: List[Dict[str, Any]], conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """
    Batch create native Archicad elements in a single undoable transaction.
    Supported `"type"` values: `"Wall"`, `"Column"`, `"Beam"`, `"Door"`, `"Window"`, `"Object"`.
    For `"Door"` and `"Window"`, pass `"wallGuid"`, `"objLoc"`, `"lower"`, `"width"`, `"height"`.
    """
    return _exec("CreateNativeElements", {"elements": elements}, conn)


# =============================================================================
# 7. GDL Parameters (API_ElementMemo Inspection & Modification)
# =============================================================================

def get_gdl_parameters(
    element_guid: Union[str, Any], conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Retrieve all GDL parameters (`API_AddParType`) from an element's `API_ElementMemo`."""
    g_str = str(element_guid.guid) if hasattr(element_guid, "guid") else str(element_guid)
    return _exec("GetSetGDLParameters", {"guid": g_str, "action": "get"}, conn)


def set_gdl_parameters(
    element_guid: Union[str, Any],
    updates: List[Dict[str, Any]],
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """
    Modify GDL parameters on an element's `API_ElementMemo`.
    Each item in `updates` should be `{"name": "<paramName>", "valueNum": 1.23}`
    or `{"name": "<paramName>", "valueStr": "text"}`.
    """
    g_str = str(element_guid.guid) if hasattr(element_guid, "guid") else str(element_guid)
    return _exec(
        "GetSetGDLParameters",
        {"guid": g_str, "action": "set", "updates": updates},
        conn,
    )


# =============================================================================
# 8. Layout Book, Cut Plane, Publisher & Database/View Control
# =============================================================================

def list_layouts(conn: Optional[ACConnection] = None) -> Dict[str, Any]:
    """List all Layout databases in the project's Layout Book."""
    return _exec("ListLayoutDatabases", {}, conn)


def place_drawing_on_layout(
    layout_guid: str,
    view_guid: str,
    pos_x: float = 0.21,
    pos_y: float = 0.1485,
    ratio: float = 1.0,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """Place a Navigator View onto a Layout sheet as an `API_DrawingID` element."""
    return _exec(
        "PlaceDrawingOnLayout",
        {
            "layoutGuid": str(layout_guid),
            "viewGuid": str(view_guid),
            "posX": float(pos_x),
            "posY": float(pos_y),
            "ratio": float(ratio),
        },
        conn,
    )


def place_drawings_batch(
    placements: List[Dict[str, Any]], conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Batch place multiple Navigator Views onto Layout sheets in one transaction."""
    return _exec("PlaceDrawingOnLayout", {"placements": placements}, conn)


def set_cut_plane(
    cut_level: float = 3.20,
    top_cut_level: Optional[float] = None,
    bottom_cut_level: Optional[float] = None,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """Set the global Floor Plan Cut Plane elevation (`currCutLevel`)."""
    params: Dict[str, Any] = {"cutLevel": float(cut_level)}
    if top_cut_level is not None:
        params["topCutLevel"] = float(top_cut_level)
    if bottom_cut_level is not None:
        params["bottomCutLevel"] = float(bottom_cut_level)
    return _exec("SetFloorPlanCutPlane", params, conn)


def publish_set(
    set_name: str = "", output_path: str = "", conn: Optional[ACConnection] = None
) -> Dict[str, Any]:
    """Trigger native Archicad Publisher Set export."""
    return _exec(
        "PublishSet", {"setName": set_name, "outputPath": output_path}, conn
    )


def database_and_view_control(
    action: str = "rebuild",
    regenerate: bool = True,
    conn: Optional[ACConnection] = None,
) -> Dict[str, Any]:
    """Execute `'rebuild'`, `'redraw'`, or `'save'` on the active Archicad project."""
    return _exec(
        "DatabaseAndViewControl",
        {"action": action, "regenerate": bool(regenerate)},
        conn,
    )


if __name__ == "__main__":
    print(f"Testing Superquadra Kernel SDK v{SDK_VERSION}...")
    print(ping())
