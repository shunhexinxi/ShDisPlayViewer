#!/usr/bin/env python3
"""Generate a fictional, asset-free rehearsal for ShDisplayViewer (Python 3.9+)."""
import argparse
import io
import math
from pathlib import Path
import xml.etree.ElementTree as ET

PLAN_ID = "example-warehouse-fire-v1"
RED, ORANGE, GREEN, BLUE, PURPLE, CYAN = (
    "#ff594f", "#ffbd4a", "#49df9c", "#47aaff", "#c28cff", "#52dce7")


def build(longitude=120.365, latitude=36.31):
    # All offsets, locations and routes are invented. No real facility is asserted.
    if not math.isfinite(longitude) or not math.isfinite(latitude):
        raise ValueError("Coordinates must be finite")
    if not (-179 < longitude < 179 and -85 < latitude < 85):
        raise ValueError("Use an anchor away from the poles and antimeridian")
    default_location = longitude == 120.365 and latitude == 36.31
    plan_name = "示例：青岛仓库火灾协同处置（虚构）" if default_location else "示例：仓库火灾协同处置（虚构）"
    area_name = "山东省青岛市虚构园区" if default_location else "虚构园区"
    area_size = "区域约东西22公里、南北13公里" if default_location else "区域横跨约0.24度经度、0.12度纬度"
    root = ET.Element("dynamicPlans", schemaVersion="1", coordinateSystem="EPSG:4326")
    plan = ET.SubElement(root, "plan", id=PLAN_ID, name=plan_name, loop="false",
        description=f"{area_name}桌面推演；{area_size}，8个步骤、30个图上标记，约2分30秒。坐标仅用于示意，不对应真实园区或可通行道路。报警、疏散等任务可并行；播放顺序不是现场操作先后要求。历史标记累积保留，以步骤编号识别。")

    def coordinate(x, y, scale=12):
        # Regional routes span the city; incident and deployment symbols use
        # smaller scales so a fictional warehouse does not become kilometers wide.
        return dict(longitude=f"{longitude+x*scale:.8f}", latitude=f"{latitude+y*scale:.8f}", altitude="0")

    def step(number, name, description, center=(0, 0), distance=4000):
        node = ET.SubElement(plan, "step", id=f"wf-step-{number:02d}", name=f"{number:02d} {name}",
            description=description, markerDisplay="sequential", markerDurationSeconds="5", durationSeconds="20")
        view = dict(coordinate(*center), heading="0", pitch="-75", range=str(distance), flyTimeSeconds="2")
        ET.SubElement(node, "viewpoint", view)
        return node, ET.SubElement(node, "markers"), view

    def marker(context, key, name, kind, points, color, description, camera=None, distance=None, scale=12):
        node, markers, view = context
        number = node.get("id").split("-")[-1]
        attributes = dict(id=f"wf-{key}", name=f"{number} {name}", geometryType=kind, category="exercise-example",
            icon="", visible="true", color=color, fillColor=color, fillOpacity="0.14", lineWidth="3",
            lineOpacity="0.95", pointSize="18", textSize="16", altitudeMode="0",
            labelBackgroundImage="", labelTextColor="#ffffffff", labelBackgroundColor="#e620354d",
            labelWidth="210", labelHeight="32", description=description, **coordinate(*points[0], scale=scale))
        camera_view = dict(view)
        if camera is not None:
            camera_view.update(coordinate(*camera))
        if distance is not None:
            camera_view["range"] = str(distance)
        for field in ["longitude", "latitude", "altitude", "heading", "pitch", "range"]:
            attributes["view"+field.capitalize()] = camera_view[field]
        attributes["viewFlyTime"] = "2"
        element = ET.SubElement(markers, "marker", attributes)
        for point in points:
            ET.SubElement(element, "point", coordinate(*point, scale=scale))
        return element

    s = step(1, "发现报警", "注入：仓库发现烟火。标记火点、报告警情并同步启动疏散。红色为初始情景记录；区域边界仅供演示。", distance=2600)
    marker(s, "initial-fire", "初始火点", "point", [(0, 0)], RED, "演练注入：A仓库发现烟火。报告地点、现场情况及人员情况；同步启动报警与疏散。不是实际警情。")
    marker(s, "warehouse", "涉事仓库（示意）", "polygon", [(-.0008,-.0005),(.0008,-.0005),(.0008,.0005),(-.0008,.0005)], RED, "虚构建筑轮廓。保留为事件初始位置，不表示后续火情仍未控制。", scale=1)
    marker(s, "attention-area", "初始关注区", "polygon", [(-.0018,-.0012),(.002,-.0012),(.002,.0012),(-.0018,.0012)], ORANGE, "用于展示影响区域标记。未计算烟气、风向、热辐射或安全距离，不能作为现场警戒边界。", scale=1)
    marker(s, "alarm-report", "警情上报", "point", [(-.0026,.0022)], ORANGE, "记录警情信息、联系和接应信息。讨论项：有哪些人员和危险因素尚待核实？", distance=3400, scale=1)

    s = step(2, "疏散接应", "单位疏散组组织撤离，集合清点；门岗接应救援力量。报警与疏散可同时开展，不等待本步播放。绿色箭头为示意方向。", center=(-.002,.001), distance=9000)
    marker(s, "evacuation-route", "疏散方向", "plot-polyline-arrow", [(-.0011,.0003),(-.0022,.0003),(-.0031,.0018),(-.0045,.0024)], GREEN, "虚构疏散线路；真实使用前必须按建筑出口、烟气和道路条件重新核实。", scale=4)
    marker(s, "assembly", "人员集合点", "point", [(-.0048,.0025)], GREEN, "疏散组在示意集合点清点人员、报告未确认人员信息。此位置不代表真实安全区。", scale=4)
    marker(s, "gate", "门岗接应", "point", [(-.003,-.0025)], CYAN, "接应组保持消防通道畅通，引导到场力量，提供园区图及已知危险因素。", scale=4)
    marker(s, "accountability", "人员信息核对", "point", [(-.0052,.0035)], GREEN, "导演提问：人员是否全部确认？未确认信息由谁反馈给现场指挥？", distance=4100, scale=4)

    s = step(3, "首批力量前往", "蓝色依次展现首批力量出发点、进场路线、途中节点和到场位置。节点表示历史行进过程，车辆没有连续移动；路线不用于导航。", center=(-.005,-.002), distance=24000)
    marker(s, "first-base", "首批力量出发", "point", [(-.010,-.005)], BLUE, "虚构消防力量A从示意驻点出动。真实编成、装备和响应时间未作设定。")
    marker(s, "first-route", "首批进场路线", "plot-polyline-arrow", [(-.010,-.005),(-.007,-.0025),(-.004,-.003),(-.001,-.000833)], BLUE, "行进过程示意。先展示路径，再展示途中及到场节点；不包含路网寻路。")
    marker(s, "first-checkpoint", "首批途中节点", "point", [(-.007,-.0025)], BLUE, "演练报告：首批力量行进至节点A，与接应人员保持联系。地图上的出发点仍作为历史记录保留。")
    marker(s, "first-arrival", "首批到场", "point", [(-.0026,-.0028)], BLUE, "到场后与接应组交接警情、人员信息和危险因素，向指挥报告。", camera=(-.001,-.001), distance=3000, scale=4)

    s = step(4, "到场部署", "建立指挥联络，核实现场情况，图示救援作业、保障和管控位置。此处只模拟任务分工，不规定内攻方式或战术安全距离。", distance=5000)
    marker(s, "command", "现场指挥点", "point", [(-.0025,.0013)], CYAN, "指挥组汇总侦察、疏散和到场报告，明确联络与增援需求。点位由真实现场条件另行确定。", scale=1)
    marker(s, "rescue-sector", "救援作业区", "polygon", [(.0013,-.001),(.0025,-.001),(.0025,0),(.0013,0)], BLUE, "专业救援力量按现场指挥开展工作。此多边形只是演练部署符号，不提供具体灭火方法。", scale=1)
    marker(s, "water-support", "供水保障核验", "point", [(.0028,-.003)], CYAN, "保障组核验可用供水条件和装备接口；该点不是实际消火栓或可用水源。", scale=1)
    marker(s, "traffic-control", "入口秩序维护", "line", [(-.004,-.0034),(-.0017,-.0034)], ORANGE, "协调维护入口秩序、保障救援通道，避免无关人员进入现场。范围仅为图示。", scale=1)

    s = step(5, "请求增援", "情景追加：首批力量报告持续保障需求。指挥协调增援，紫色依次展示请求、出发、路线和途中节点。", center=(.003,.002), distance=25000)
    marker(s, "request-support", "报告增援需求", "point", [(-.0015,.003)], PURPLE, "导演注入：需要补充保障与机动力量。报告任务、现有能力、缺口、集结位置和联络方式。", scale=1)
    marker(s, "support-base", "增援力量出发", "point", [(.010,.005)], PURPLE, "虚构支援力量B接到协同任务，从示意驻点出发；编成不代表真实标准。")
    marker(s, "support-route", "增援进场路线", "plot-polyline-arrow", [(.010,.005),(.007,.005),(.007,.0015),(.0045,.0015),(.0045,-.0015)], PURPLE, "虚构增援线路，与首批力量示意进场方向区分。真实道路、通行限制需重新核实。")
    marker(s, "support-checkpoint", "增援途中节点", "point", [(.007,.0015)], PURPLE, "演练报告：支援力量至节点B，确认接应点及联络信息。")

    s = step(6, "增援协同", "增援到场交接任务，医疗、装备与联络保障同步展开。各点为角色部署示意，不表示真实单位或设施。", center=(.001,.001), distance=14000)
    marker(s, "support-arrival", "增援集结交接", "point", [(.0045,-.0015)], PURPLE, "增援力量到达示意集结点，登记、交接任务并纳入统一指挥。")
    marker(s, "medical", "医疗协同点", "point", [(-.004,.005)], GREEN, "示意医疗力量接应、信息交接与转运协调。未设置伤情诊断或医疗处置方案。", scale=4)
    marker(s, "logistics", "装备保障点", "point", [(.005,-.0035)], CYAN, "装备与补给保障，登记领用及机动力量状态。", scale=4)
    marker(s, "liaison", "协同信息汇总", "point", [(.002,.005)], CYAN, "导演提问：增援是否收到同一任务口径？人员清点、救援和保障信息是否闭环？", scale=4)

    s = step(7, "火情控制", "情景追加：指挥接报火情已控制。绿色状态点记录此时结论；红色初始火点仍作为历史记录。继续按指挥要求核查和看护。", center=(0,.0005), distance=3200)
    marker(s, "controlled", "火情已控制", "point", [(.0005,.0008)], GREEN, "演练状态更新：根据导演注入，火情已控制。该状态不是由软件传感或火灾模型自动判定。", scale=1)
    marker(s, "recheck", "现场复核", "point", [(.0018,.002)], GREEN, "专业人员按现场指挥要求开展复核，反馈尚存风险，保留必要管控。", scale=1)
    marker(s, "watch", "后续看护交接", "point", [(-.0018,-.0018)], CYAN, "记录后续看护、联络和交接责任；解除措施由真实现场指挥决定。", scale=1)

    s = step(8, "清点复盘", "完成各组人员与装备清点、信息交接及复盘。可回看任一步骤，讨论报警信息、行进接应、增援和协同中的缺口。", distance=100000)
    marker(s, "final-accountability", "人员清点完成", "point", [(-.0055,.001)], GREEN, "各组汇报人员清点结果，核对进入、退出和交接记录。演练不预设真实伤亡数字。", scale=4)
    marker(s, "equipment-return", "装备清点交接", "point", [(.006,-.0025)], CYAN, "登记装备回收、缺损和补充需求，完成后续任务交接。", scale=4)
    marker(s, "debrief", "演练复盘", "point", [(-.003,.006)], GREEN, "复盘问题：报警是否清楚？人员信息是否闭环？首批与增援是否顺利接应？地图标记、路线和视角是否清晰？", scale=4)

    for node in plan.findall("step"):
        node.set("durationSeconds", str(len(node.findall("markers/marker"))*5))
    return root


def validate(root):
    plan = root.find("plan")
    assert len(root.findall("plan")) == 1 and plan.get("id") == PLAN_ID
    assert len(plan.findall("step")) == 8
    markers = plan.findall("step/markers/marker")
    assert len(markers) == 30
    ids = [node.get("id") for node in root.iter() if node.get("id")]
    assert len(ids) == len(set(ids)), "Duplicate IDs"
    first_views = []
    for node in plan.findall("step"):
        viewpoint = node.find("viewpoint")
        first = node.find("markers/marker")
        assert first is not None and viewpoint is not None
        first_view = tuple(float(first.get("view"+field)) for field in ("Longitude", "Latitude", "Range"))
        assert first_view == tuple(float(viewpoint.get(field)) for field in ("longitude", "latitude", "range"))
        first_views.append(first_view)
        for item in node.findall("markers/marker"):
            assert float(item.get("viewFlyTime")) + .5 <= int(node.get("markerDurationSeconds"))
            assert float(item.get("viewRange")) > 0
            points = item.findall("point")
            minimum = 3 if item.get("geometryType") == "polygon" else 2 if item.get("geometryType") in ("line", "plot-polyline-arrow") else 1
            assert len(points) >= minimum
            for point in points:
                assert -180 <= float(point.get("longitude")) <= 180
                assert -90 <= float(point.get("latitude")) <= 90
    assert all(a != b for a, b in zip(first_views, first_views[1:])), "Adjacent steps need distinct cameras"
    return sum(int(n.get("markerDurationSeconds"))*len(n.findall("markers/marker")) for n in plan.findall("step"))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--longitude", type=float, default=120.365)
    parser.add_argument("--latitude", type=float, default=36.31)
    args = parser.parse_args()
    document = build(args.longitude, args.latitude)
    seconds = validate(document)
    ET.indent(document, space="  ")
    destination = Path(__file__).with_name("dynamic-plans.xml")
    # Write bytes explicitly so Windows and Unix produce the same LF XML.
    output = io.BytesIO()
    ET.ElementTree(document).write(output, encoding="utf-8", xml_declaration=True)
    destination.write_bytes(output.getvalue())
    print(f"{destination.name}: 1 plan, 8 steps, 30 markers, {seconds} seconds")
