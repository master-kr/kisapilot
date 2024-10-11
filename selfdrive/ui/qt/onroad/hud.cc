#include "selfdrive/ui/qt/onroad/hud.h"

#include <cmath>
#include <unistd.h> // kisapilot
#include <QDateTime>

#include "selfdrive/ui/qt/util.h"

constexpr int SET_SPEED_NA = 255;

HudRenderer::HudRenderer() {
  const int img_size = (192 / 4) * 3;
  engage_img = loadPixmap("../assets/img_chffr_wheel.png", {img_size, img_size});
  experimental_img = loadPixmap("../assets/img_experimental.svg", {img_size + 5, img_size + 5});
  gear_img_p = loadPixmap("../assets/addon/img/circle_red_letter-p.svg", {img_size+45, img_size+45});
  gear_img_r = loadPixmap("../assets/addon/img/circle_green_letter-r.svg", {img_size+45, img_size+45});
  gear_img_n = loadPixmap("../assets/addon/img/circle_blue_letter-n.svg", {img_size+45, img_size+45});
  gear_img_d = loadPixmap("../assets/addon/img/circle_green_letter-d.svg", {img_size+45, img_size+45});
  kisapilot_img = loadPixmap("../assets/addon/img/kisapilot.png", {img_size-35, img_size-35});
  waze_police_img = loadPixmap("../assets/addon/img/img_police_car.png", {img_size+45, img_size+45});
  waze_cam_img = loadPixmap("../assets/addon/img/img_speed_cam.png", {img_size+45, img_size+45});
}

void HudRenderer::updateState(const UIState &s) {
  is_metric = s.scene.is_metric;
  status = s.status;

  const SubMaster &sm = *(s.sm);
  if (!sm.alive("carState")) {
    is_cruise_set = false;
    set_speed = SET_SPEED_NA;
    speed = 0.0;
    return;
  }

  const auto &controls_state = sm["controlsState"].getControlsState();
  const auto &car_state = sm["carState"].getCarState();

  // Handle older routes where vCruiseCluster is not set
  set_speed = car_state.getVCruiseCluster() == 0.0 ? controls_state.getVCruiseDEPRECATED() : car_state.getVCruiseCluster();
  is_cruise_set = set_speed > 0 && set_speed != SET_SPEED_NA;

  // if (is_cruise_set && !is_metric) {
  //   set_speed *= KM_TO_MILE;
  // }

  // Handle older routes where vEgoCluster is not set
  v_ego_cluster_seen = v_ego_cluster_seen || car_state.getVEgoCluster() != 0.0;
  float v_ego = v_ego_cluster_seen ? car_state.getVEgoCluster() : car_state.getVEgo();
  speed = std::max<float>(0.0f, v_ego * (is_metric ? MS_TO_KPH : MS_TO_MPH));

  over_sl = s.scene.limitSpeedCamera > 19 && ((s.scene.car_state.getVEgo() * (is_metric ? MS_TO_KPH : MS_TO_MPH)) > s.scene.ctrl_speed+1.5);
  auto lead_one = sm["radarState"].getRadarState().getLeadOne();
  if (s.scene.radarDRel < 149 && s.scene.use_radar_value) {
    dist_rel = s.scene.radarDRel;
    vel_rel = s.scene.radarVRel;
  } else {
    dist_rel = lead_one.getDRel();
    vel_rel = lead_one.getVRel();
  }
  lead_stat = lead_one.getStatus();
}

void HudRenderer::draw(QPainter &p, const QRect &surface_rect) {
  p.save();

  // Draw header gradient
  QLinearGradient bg(0, UI_HEADER_HEIGHT - (UI_HEADER_HEIGHT / 2.5), 0, UI_HEADER_HEIGHT);
  bg.setColorAt(0, QColor::fromRgbF(0, 0, 0, 0.45));
  bg.setColorAt(1, QColor::fromRgbF(0, 0, 0, 0));
  p.fillRect(0, 0, surface_rect.width(), UI_HEADER_HEIGHT, bg);


  drawSetSpeed(p, surface_rect);
  drawCurrentSpeed(p, surface_rect);
  drawWheelState(p, surface_rect);


  // kisapilot
  UIState *s = uiState();
  p.setBrush(QColor(0, 0, 0, 0));
  p.setPen(whiteColor(150));
  //p.setRenderHint(QPainter::TextAntialiasing);
  p.setOpacity(0.7);
  int ui_viz_rx = UI_BORDER_SIZE + 190;
  int ui_viz_ry = s->scene.low_ui_profile?UI_BORDER_SIZE - 200:UI_BORDER_SIZE + 100;
  int ui_viz_rx_center = s->fb_w/2;

  // debug
  int debug_y1 = 1015-UI_BORDER_SIZE-(s->scene.animated_rpm?60:0);
  int debug_y2 = 1050-UI_BORDER_SIZE-(s->scene.animated_rpm?60:0);
  int debug_y3 = 981-UI_BORDER_SIZE-(s->scene.animated_rpm?60:0);
  if (s->scene.nDebugUi1 && s->scene.comma_stock_ui != 1) {
    p.setFont(InterFont(30, QFont::DemiBold));
    uiText(p, s->scene.low_ui_profile?320:205, debug_y1, s->scene.alertTextMsg1.c_str());
    uiText(p, s->scene.low_ui_profile?320:205, debug_y2, s->scene.alertTextMsg2.c_str());
    uiText(p, s->scene.low_ui_profile?320:205, debug_y3, s->scene.alertTextMsg3.c_str());
  }
  if (s->scene.nDebugUi3 && s->scene.comma_stock_ui != 1) {
    p.setFont(InterFont(35, QFont::DemiBold));
    uiText(p, ui_viz_rx+400, ui_viz_ry+560,
    "0: " + QString::number(s->scene.longitudinalPlan.lead0[0], 'f', 2) +
    " "   + QString::number(s->scene.longitudinalPlan.lead0[12], 'f', 2));
    uiText(p, ui_viz_rx+400, ui_viz_ry+600,
    "1: " + QString::number(s->scene.longitudinalPlan.lead1[0], 'f', 2) +
    " "   + QString::number(s->scene.longitudinalPlan.lead1[12], 'f', 2));
    uiText(p, ui_viz_rx+400, ui_viz_ry+640,
    "C: " + QString::number(s->scene.longitudinalPlan.cruisetg[0], 'f', 2) +
    " "   + QString::number(s->scene.longitudinalPlan.cruisetg[12], 'f', 2));
    uiText(p, ui_viz_rx+400, ui_viz_ry+680,
    "X: " + QString::number(s->scene.longitudinalPlan.e2ex[0], 'f', 2) +
    " "   + QString::number(s->scene.longitudinalPlan.e2ex[12], 'f', 2));
  }
  if (s->scene.KISA_Debug && s->scene.comma_stock_ui != 1) {
    p.setFont(InterFont(35, QFont::DemiBold));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+240:ui_viz_ry+280, "CAR:" + QString::fromStdString(s->scene.car_fingerprint));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+280:ui_viz_ry+320, "PSM:" + QString::fromStdString(s->scene.selfdrive_state.getPandaSafetyModel()) +
     "/ISM:" + QString::fromStdString(s->scene.selfdrive_state.getInterfaceSafetyModel()));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+320:ui_viz_ry+360, "RXC:" + QString::number(int(s->scene.selfdrive_state.getRxChecks())) +
     "/MCT:" + QString::number(int(s->scene.selfdrive_state.getMismatchCounter())));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+360:ui_viz_ry+400, "PTY:" + QString::number(int(s->scene.pandaType)) +
     "/IGN:" + QString::number(int(s->scene.ignition)));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+400:ui_viz_ry+440, "CAW:" + QString::number(int(s->scene.controlAllowed)) +
     "/ENA:" + QString::number(int(s->scene.enabled)));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+440:ui_viz_ry+480, "STK:" + QString::number(int(s->scene.stock_lkas_on_disengagement)) +
     "/UFC:" + QString::number(int(s->scene.ufc_mode)));
    uiText(p, ui_viz_rx+400, s->scene.low_ui_profile?ui_viz_ry+480:ui_viz_ry+520, "MDL:" + s->scene.model_name);
  }
  if (s->scene.nDebugUi2 && s->scene.comma_stock_ui != 1) {
    p.setFont(InterFont(35, QFont::DemiBold));
    if (!s->scene.low_ui_profile) {
      uiText(p, ui_viz_rx, ui_viz_ry+240, "SR:" + QString::number(s->scene.liveParams.steerRatio, 'f', 2));
    } else {
      uiText(p, ui_viz_rx, ui_viz_ry+240, QString::number(s->scene.liveParams.steerRatio, 'f', 2) + "|" + QString::number(s->scene.steerRatio, 'f', 2));
    }
    uiText(p, ui_viz_rx, ui_viz_ry+280, "AA:" + QString::number(s->scene.liveParams.angleOffsetAverage, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+320, "SF:" + QString::number(s->scene.liveParams.stiffnessFactor, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+360, "AD:" + QString::number(s->scene.steer_actuator_delay, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+400, "OS:" + QString::number(s->scene.output_scale, 'f', 2));
    uiText(p, ui_viz_rx, ui_viz_ry+440, QString::number(s->scene.lateralPlan.dProb, 'f', 1) + "/" + QString::number(s->scene.lateralPlan.laneWidth, 'f', 1) + "m"
                                + "/" + QString::number(s->scene.lateralPlan.totalCameraOffset, 'f', 2));

    if (!s->scene.low_ui_profile) {
      QString szLaCMethod = "";
      QString szLaCMethodCur = "";
      switch(s->scene.lateralControlMethod) {
        case 0: szLaCMethod = "PID"; break;
        case 1: szLaCMethod = "INDI"; break;
        case 2: szLaCMethod = "LQR"; break;
        case 3: szLaCMethod = "TORQUE"; break;
        case 4: szLaCMethod = "MULTI"; break;
      }
      switch((int)s->scene.multi_lat_selected) {
        case 0: szLaCMethodCur = "PID"; break;
        case 1: szLaCMethodCur = "INDI"; break;
        case 2: szLaCMethodCur = "LQR"; break;
        case 3: szLaCMethodCur = "TORQUE"; break;
      }
      if (!s->scene.animated_rpm) {
        if (szLaCMethod != "") drawText(p, ui_viz_rx_center, UI_BORDER_SIZE+305, szLaCMethod);
        if (s->scene.lateralControlMethod == 4) {
          if( szLaCMethodCur != "") drawText(p, ui_viz_rx_center, UI_BORDER_SIZE+345, szLaCMethodCur);
        }
      } else {
        if(szLaCMethod != "") drawText(p, ui_viz_rx_center, UI_BORDER_SIZE+340, szLaCMethod);
        if (s->scene.lateralControlMethod == 4) {
          if(szLaCMethodCur != "") drawText(p, ui_viz_rx_center, UI_BORDER_SIZE+375, szLaCMethodCur);
        }
      }
    }
    if (s->scene.navi_select == 1) {
      if (s->scene.liveENaviData.ekisasafetysign) uiText(p, ui_viz_rx, ui_viz_ry+560, "CS:" + QString::number(s->scene.liveENaviData.ekisasafetysign, 'f', 0));
      if (s->scene.liveENaviData.ekisasafetydist) uiText(p, ui_viz_rx, ui_viz_ry+600, "SL:" + QString::number(s->scene.liveENaviData.ekisaspeedlimit, 'f', 0) + "/DS:" + QString::number(s->scene.liveENaviData.ekisasafetydist, 'f', 0));
      if (s->scene.liveENaviData.ekisaturninfo) uiText(p, ui_viz_rx, ui_viz_ry+640, "TI:" + QString::number(s->scene.liveENaviData.ekisaturninfo, 'f', 0) + "/DT:" + QString::number(s->scene.liveENaviData.ekisadisttoturn, 'f', 0));
      if (s->scene.liveENaviData.ekisaroadlimitspeed > 0 && s->scene.liveENaviData.ekisaroadlimitspeed < 200) uiText(p, ui_viz_rx, ui_viz_ry+680, "RS:" + QString::number(s->scene.liveENaviData.ekisaroadlimitspeed, 'f', 0));
      if (s->scene.liveENaviData.ekisaishighway || s->scene.liveENaviData.ekisaistunnel) uiText(p, ui_viz_rx, ui_viz_ry+720, "H:" + QString::number(s->scene.liveENaviData.ekisaishighway, 'f', 0) + "/T:" + QString::number(s->scene.liveENaviData.ekisaistunnel, 'f', 0));
    } else if (s->scene.navi_select == 2) {
      if (s->scene.liveENaviData.ewazealertdistance) uiText(p, ui_viz_rx, ui_viz_ry+560, "AS:" + QString::number(s->scene.liveENaviData.ewazealertid, 'f', 0) + "/DS:" + QString::number(s->scene.liveENaviData.ewazealertdistance, 'f', 0));
      if (s->scene.liveENaviData.ewazealertdistance) uiText(p, ui_viz_rx, ui_viz_ry+600, "T:" + QString::fromStdString(s->scene.liveENaviData.ewazealerttype));
      if (s->scene.liveENaviData.ewazecurrentspeed || s->scene.liveENaviData.ewazeroadspeedlimit) uiText(p, ui_viz_rx, ui_viz_ry+640, "CS:" + QString::number(s->scene.liveENaviData.ewazecurrentspeed, 'f', 0) + "/RS:" + QString::number(s->scene.liveENaviData.ewazeroadspeedlimit, 'f', 0));
      if (s->scene.liveENaviData.ewazenavsign) uiText(p, ui_viz_rx, ui_viz_ry+680, "NS:" + QString::number(s->scene.liveENaviData.ewazenavsign, 'f', 0));
      if (s->scene.liveENaviData.ewazenavdistance) uiText(p, ui_viz_rx, ui_viz_ry+720, "ND:" + QString::number(s->scene.liveENaviData.ewazenavdistance, 'f', 0));
    }
    if (s->scene.osm_enabled && !s->scene.KISA_Debug) {
      uiText(p, ui_viz_rx+200, ui_viz_ry+240, "SL:" + QString::number(s->scene.liveMapData.ospeedLimit, 'f', 0));
      uiText(p, ui_viz_rx+200, ui_viz_ry+280, "SLA:" + QString::number(s->scene.liveMapData.ospeedLimitAhead, 'f', 0));
      uiText(p, ui_viz_rx+200, ui_viz_ry+320, "SLAD:" + QString::number(s->scene.liveMapData.ospeedLimitAheadDistance, 'f', 0));
      uiText(p, ui_viz_rx+200, ui_viz_ry+360, "TSL:" + QString::number(s->scene.liveMapData.oturnSpeedLimit, 'f', 0));
      uiText(p, ui_viz_rx+200, ui_viz_ry+400, "TSLED:" + QString::number(s->scene.liveMapData.oturnSpeedLimitEndDistance, 'f', 0));
      uiText(p, ui_viz_rx+200, ui_viz_ry+440, "TSLS:" + QString::number(s->scene.liveMapData.oturnSpeedLimitSign, 'f', 0));
    }
  }

  p.setOpacity(1.0);
  if (s->scene.comma_stock_ui != 1) {
    int j_num = 100;
    // debug info(left panel)
    int width_l = 180;
    int sp_xl = surface_rect.left() + UI_BORDER_SIZE + width_l / 2 - 10;
    int sp_yl = UI_BORDER_SIZE + 255;
    int num_l = 0;

    //p.setRenderHint(QPainter::TextAntialiasing);
    // lead drel
    num_l = num_l + 1;
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl, QString("REL DIST"), 150, 27);
    if (lead_stat) {
      if (dist_rel < 5) {
        p.setPen(redColor(200));
      } else if (int(dist_rel) < 15) {
        p.setPen(orangeColor(200));
      }
      if (dist_rel < 10) {
        debugText(p, sp_xl, sp_yl+60, QString::number(dist_rel, 'f', 1), 150, 57);
      } else {
        debugText(p, sp_xl, sp_yl+60, QString::number(dist_rel, 'f', 0), 150, 57);
      }
    } else {
      debugText(p, sp_xl, sp_yl+60, "-", 150, 57);
    }
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    p.setPen(whiteColor(200));
    p.setFont(InterFont(27, QFont::DemiBold));
    p.drawText(-25, 0, "m");
    p.resetMatrix();

    // lead spd
    num_l = num_l + 1;
    sp_yl = sp_yl + j_num;
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl, QString("REL Speed"), 150, 27);
    if (int(vel_rel) < -5) {
      p.setPen(redColor(200));
    } else if (int(vel_rel) < 0) {
      p.setPen(orangeColor(200));
    }
    if (lead_stat) {
      debugText(p, sp_xl, sp_yl+60, QString::number(vel_rel * (s->scene.is_metric ? 3.6 : 2.2369363), 'f', 0), 150, 57);
    } else {
      debugText(p, sp_xl, sp_yl+60, "-", 150, 57);
    }
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    p.setPen(whiteColor(200));
    p.setFont(InterFont(27, QFont::DemiBold));
    if (s->scene.is_metric) {p.drawText(-50, 0, "km/h");} else {p.drawText(-50, 0, "mi/h");}
    p.resetMatrix();

    // steer angle
    num_l = num_l + 1;
    sp_yl = sp_yl + j_num;
    p.setPen(whiteColor(200));
    debugText(p, sp_xl, sp_yl, QString("Steer ANG"), 150, 27);
    p.setPen(greenColor(200));
    if ((int(s->scene.angleSteers) < -50) || (int(s->scene.angleSteers) > 50)) {
      p.setPen(redColor(200));
    } else if ((int(s->scene.angleSteers) < -30) || (int(s->scene.angleSteers) > 30)) {
      p.setPen(orangeColor(200));
    }
    if (s->scene.angleSteers > -10 && s->scene.angleSteers < 10) {
      debugText(p, sp_xl, sp_yl+60, QString::number(s->scene.angleSteers, 'f', 1), 150, 57);
    } else {
      debugText(p, sp_xl, sp_yl+60, QString::number(s->scene.angleSteers, 'f', 0), 150, 57);
    }
    p.translate(sp_xl + 90, sp_yl + 20);
    p.rotate(-90);
    p.setPen(whiteColor(200));
    p.setFont(InterFont(27, QFont::DemiBold));
    if (s->scene.desired_angle_steers > -10 && s->scene.desired_angle_steers < 10) {
      p.drawText(-40, 0, QString::number(s->scene.desired_angle_steers, 'f', 1));
    } else {
      p.drawText(-50, 0, QString::number(s->scene.desired_angle_steers, 'f', 0));
    }
    p.resetMatrix();
    // steer ratio
    if (!s->scene.low_ui_profile) {
      num_l = num_l + 1;
      sp_yl = sp_yl + j_num;
      debugText(p, sp_xl, sp_yl, QString("SteerRatio"), 150, 27);
      debugText(p, sp_xl, sp_yl+60, QString::number(s->scene.steerRatio, 'f', 2), 150, 57);
    }

    // gear step and cruise gap
    // if (0 < s->scene.gear_step && s->scene.gear_step < 9) {
    if (true) {
      num_l = num_l + 1;
      sp_yl = sp_yl + j_num;
      if (s->scene.charge_meter > 0) {
        p.setPen(whiteColor(200));
        debugText(p, sp_xl, sp_yl, QString("MAIN BAT"), 150, 27);
        p.setPen(yellowColor(230));
        debugText(p, sp_xl, sp_yl+60, QString::number(s->scene.charge_meter, 'f', 0) + "%", 150, 57);
      } else {
        p.setPen(whiteColor(200));
        debugText(p, sp_xl, sp_yl, QString("GEAR"), 150, 27);
        p.setPen(yellowColor(230));
        debugText(p, sp_xl, sp_yl+60, "D " + QString::number(s->scene.gear_step, 'f', 0), 150, 57);
      }
      p.translate(sp_xl + 90, sp_yl + 20);
      p.rotate(-90);
      p.setFont(InterFont(27, QFont::DemiBold));
      if (s->scene.cruise_gap == 1) {
        if (s->scene.gap_by_speed_on) {
          p.setPen(QColor(0, 180, 255, 220));
          p.drawText(-20, 0, "■");
        } else {
          p.setPen(redColor(200));
          p.drawText(-20, 0, "■");
        }
      } else if (s->scene.cruise_gap == 2) {
        if (s->scene.gap_by_speed_on) {
          p.setPen(QColor(0, 180, 255, 220));
          p.drawText(-30, 0, "■■");
        } else {
          p.setPen(redColor(200));
          p.drawText(-30, 0, "■■");
        }
      } else if (s->scene.cruise_gap == 3) {
        if (s->scene.gap_by_speed_on) {
          p.setPen(QColor(0, 180, 255, 220));
          p.drawText(-40, 0, "■■■");
        } else {
          p.setPen(greenColor(200));
          p.drawText(-40, 0, "■■■");
        }
      } else {
        if (s->scene.gap_by_speed_on) {
          p.setPen(QColor(0, 180, 255, 220));
          p.drawText(-50, 0, "■■■■");
        } else {
          p.setPen(whiteColor(200));
          p.drawText(-50, 0, "■■■■");
        }
      }
      p.resetMatrix();
    }

    QRect left_panel(surface_rect.left() + UI_BORDER_SIZE, UI_BORDER_SIZE + 215, width_l, 104*num_l);
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawRoundedRect(left_panel, 20, 20);
    // left panel end

    // debug info(right panel)
    int width_r = 180;
    int sp_xr = surface_rect.right() - UI_BORDER_SIZE - width_r / 2 - 10;
    int sp_yr = UI_BORDER_SIZE + 235;
    int num_r = 0;

    //p.setRenderHint(QPainter::TextAntialiasing);
    // cpu temp
    num_r = num_r + 1;
    p.setPen(whiteColor(200));
    debugText(p, sp_xr, sp_yr, QString("CPU TEMP"), 150, 27);
    if (s->scene.cpuTemp > 85) {
      p.setPen(redColor(200));
    } else if (s->scene.cpuTemp > 75) {
      p.setPen(orangeColor(200));
    }
    debugText(p, sp_xr, sp_yr+60, QString::number(s->scene.cpuTemp, 'f', 0) + "°C", 150, 57);
    p.translate(sp_xr + 90, sp_yr + 20);
    p.rotate(-90);
    p.setFont(InterFont(27, QFont::DemiBold));
    p.setPen(whiteColor(200));
    p.drawText(-40, 0, QString::number(s->scene.cpuPerc, 'f', 0) + "%");
    p.resetMatrix();

    // sys temp
    num_r = num_r + 1;
    sp_yr = sp_yr + j_num;
    p.setPen(whiteColor(200));
    debugText(p, sp_xr, sp_yr, QString("AMB TEMP"), 150, 27);
    if (s->scene.ambientTemp > 70) {
      p.setPen(redColor(200));
    } else if (s->scene.ambientTemp > 60) {
      p.setPen(orangeColor(200));
    } 
    debugText(p, sp_xr, sp_yr+60, QString::number(s->scene.ambientTemp, 'f', 0) + "°C", 150, 57);
    p.translate(sp_xr + 90, sp_yr + 20);
    p.rotate(-90);
    p.setFont(InterFont(27, QFont::DemiBold));
    p.setPen(whiteColor(200));
    p.drawText(-50, 0, QString::number(s->scene.fanSpeedRpm, 'f', 0));
    p.resetMatrix();

    // Ublox GPS accuracy
    num_r = num_r + 1;
    sp_yr = sp_yr + j_num;
    p.setPen(whiteColor(200));
    debugText(p, sp_xr, sp_yr, QString("GPS PREC"), 150, 27);
    if (s->scene.gpsAccuracy > 5) {
      p.setPen(redColor(200));
    } else if (s->scene.gpsAccuracy > 2.5) {
      p.setPen(orangeColor(200));
    }
    if (s->scene.gpsAccuracy > 99 || s->scene.gpsAccuracy == 0) {
      debugText(p, sp_xr, sp_yr+60, "None", 150, 52);
    } else if (s->scene.gpsAccuracy > 9.99) {
      debugText(p, sp_xr, sp_yr+60, QString::number(s->scene.gpsAccuracy, 'f', 1), 150, 57);
    } else {
      debugText(p, sp_xr, sp_yr+60, QString::number(s->scene.gpsAccuracy, 'f', 2), 150, 57);
    }
    p.translate(sp_xr + 90, sp_yr + 20);
    p.rotate(-90);
    p.setFont(InterFont(27, QFont::DemiBold));
    p.setPen(whiteColor(200));
    p.drawText(-35, 0, QString::number(s->scene.satelliteCount, 'f', 0));
    p.resetMatrix();
    // altitude
    num_r = num_r + 1;
    sp_yr = sp_yr + j_num;
    p.setPen(whiteColor(200));
    debugText(p, sp_xr, sp_yr, QString("ST USAGE"), 150, 27);
    debugText(p, sp_xr, sp_yr+60, QString::number(s->scene.storageUsage, 'f', 0) + "%", 150, 57);
    p.translate(sp_xr + 90, sp_yr + 20);
    p.rotate(-90);
    p.setFont(InterFont(27, QFont::DemiBold));
    p.drawText(-45, 0, QString::number(s->scene.altitude, 'f', 0) + "m");
    p.resetMatrix();

    // kisapilot tpms
    num_r = num_r + 1;
    int tpms_width = 180;
    int tpms_sp_xr = surface_rect.right() - UI_BORDER_SIZE - tpms_width / 2;
    int tpms_sp_yr = sp_yr + j_num - 10;
    // QRect tpms_panel(surface_rect.right() - UI_BORDER_SIZE - tpms_width, tpms_sp_yr - 25, tpms_width, 135);  
    // p.setOpacity(1.0);
    // p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    // p.drawRoundedRect(tpms_panel, 20, 20);
    p.setPen(whiteColor(200));
    //p.setRenderHint(QPainter::TextAntialiasing);
    float maxv = 0;
    float minv = 300;
    int font_size = 60;

    if (maxv < s->scene.tpmsPressureFl) {maxv = s->scene.tpmsPressureFl;}
    if (maxv < s->scene.tpmsPressureFr) {maxv = s->scene.tpmsPressureFr;}
    if (maxv < s->scene.tpmsPressureRl) {maxv = s->scene.tpmsPressureRl;}
    if (maxv < s->scene.tpmsPressureRr) {maxv = s->scene.tpmsPressureRr;}
    if (minv > s->scene.tpmsPressureFl) {minv = s->scene.tpmsPressureFl;}
    if (minv > s->scene.tpmsPressureFr) {minv = s->scene.tpmsPressureFr;}
    if (minv > s->scene.tpmsPressureRl) {minv = s->scene.tpmsPressureRl;}
    if (minv > s->scene.tpmsPressureRr) {minv = s->scene.tpmsPressureRr;}

    if (((maxv - minv) > 3 && s->scene.tpmsUnit != 2) || ((maxv - minv) > 0.2 && s->scene.tpmsUnit == 2)) {
      p.setPen(redColor(200));
    }
    if (s->scene.tpmsUnit != 0) {
      debugText(p, tpms_sp_xr, tpms_sp_yr+15, (s->scene.tpmsUnit == 2) ? "TPMS(bar)" : "TPMS(psi)", 150, 30);
      font_size = (s->scene.tpmsUnit == 2) ? 43 : 36;
    } else {
      debugText(p, tpms_sp_xr, tpms_sp_yr+15, "TPMS(psi)", 150, 30);
      font_size = 42;
    }
    if ((s->scene.tpmsPressureFl < 32 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureFl < 2.2 && s->scene.tpmsUnit == 2)) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else if (s->scene.tpmsPressureFl > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, "N/A", 200, font_size);
    } else if ((s->scene.tpmsPressureFl > 45 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureFl > 2.8 && s->scene.tpmsUnit == 2)) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    }
    if ((s->scene.tpmsPressureFr < 32 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureFr < 2.2 && s->scene.tpmsUnit == 2)) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else if (s->scene.tpmsPressureFr > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, "N/A", 200, font_size);
    } else if ((s->scene.tpmsPressureFr > 45 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureFr > 2.8 && s->scene.tpmsUnit == 2)) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+60, QString::number(s->scene.tpmsPressureFr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    }
    if ((s->scene.tpmsPressureRl < 32 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureRl < 2.2 && s->scene.tpmsUnit == 2)) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else if (s->scene.tpmsPressureRl > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, "N/A", 200, font_size);
    } else if ((s->scene.tpmsPressureRl > 45 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureRl > 2.8 && s->scene.tpmsUnit == 2)) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr-(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRl, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    }
    if ((s->scene.tpmsPressureRr < 32 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureRr < 2.2 && s->scene.tpmsUnit == 2)) {
      p.setPen(yellowColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else if (s->scene.tpmsPressureRr > 50) {
      p.setPen(whiteColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, "N/A", 200, font_size);
    } else if ((s->scene.tpmsPressureRr > 45 && s->scene.tpmsUnit != 2) || (s->scene.tpmsPressureRr > 2.8 && s->scene.tpmsUnit == 2)) {
      p.setPen(redColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    } else {
      p.setPen(greenColor(200));
      debugText(p, tpms_sp_xr+(s->scene.tpmsUnit != 0?46:50), tpms_sp_yr+100, QString::number(s->scene.tpmsPressureRr, 'f', (s->scene.tpmsUnit != 0?1:0)), 200, font_size);
    }

    QRect right_panel(surface_rect.right() - UI_BORDER_SIZE - width_r, UI_BORDER_SIZE + 195, width_r, 104*num_r+25);  
    p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    p.drawRoundedRect(right_panel, 20, 20);
    // right panel end

  }


  if (s->scene.comma_stock_ui != 1) {
    // kisapilot rec
    // QRect recbtn_draw(surface_rect.right() - UI_BORDER_SIZE - 140 - 20, 905, 140, 140);
    // p.setBrush(Qt::NoBrush);
    // if (s->scene.rec_stat) p.setBrush(redColor(150));
    // p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    // p.drawEllipse(recbtn_draw);
    // p.setPen(whiteColor(200));
    // p.setFont(InterFont(41, QFont::DemiBold));
    
    // p.drawText(recbtn_draw, Qt::AlignCenter, QString("REC"));

    // kisapilot multi menu
    int m_btn_size = 160;
    int m_btn_offset = 10;
    int m_btn_center_x = surface_rect.right() - UI_BORDER_SIZE - m_btn_size / 2 - m_btn_offset;
    int m_btn_center_y = s->scene.low_ui_profile?(UI_BORDER_SIZE + m_btn_size / 2 + m_btn_offset):(surface_rect.height()-UI_BORDER_SIZE-m_btn_size/2-m_btn_offset);
    int m_x = m_btn_center_x - m_btn_size/2;
    int m_y = m_btn_center_y - m_btn_size/2;
    QRect multi_btn_draw(m_x, m_y, m_btn_size, m_btn_size);

    drawIcon(p, m_btn_center_x, m_btn_center_y, kisapilot_img, QColor(0, 0, 0, 20), 1.0);
    p.setBrush(blackColor(60));
    p.setPen(Qt::NoPen);
    p.drawEllipse(m_x-15, m_y-15, m_btn_size+30, m_btn_size+30);
    if (s->scene.liveENaviData.ekisaconalive) {
      p.setPen(QPen(QColor(0, 255, 0, 180), 7));
    } else {
      p.setPen(QPen(QColor(255, 255, 255, 80), 6));
    }
    p.setBrush(Qt::NoBrush);
    //if (s->scene.lateralPlan.lanelessModeStatus) p.setBrush(QColor(13, 177, 248, 100));
    p.drawEllipse(multi_btn_draw);
    p.setPen(whiteColor(200));
    p.setFont(InterFont(43, QFont::DemiBold));
    //p.drawText(multi_btn_draw, Qt::AlignCenter, QString(""));
    p.setBrush(Qt::NoBrush);

    if (s->scene.multi_btn_touched) {
      s->scene.multi_btn_slide_timer += 20;
      s->scene.multi_btn_slide_timer = fmin(s->scene.multi_btn_slide_timer, 180);
      QRect multi_btn_draw1(m_x-(int)s->scene.multi_btn_slide_timer, m_y, m_btn_size, m_btn_size);
      QRect multi_btn_draw2(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y, m_btn_size, m_btn_size);
      QRect multi_btn_draw3(m_x-(int)s->scene.multi_btn_slide_timer*3, m_y, m_btn_size, m_btn_size);
      if(s->scene.rec_stat3) p.setBrush(redColor(125));
      p.drawEllipse(multi_btn_draw1);
      p.setBrush(Qt::NoBrush);
      if (s->scene.lateralPlan.lanelessModeStatus) p.setBrush(QColor(13, 177, 248, 100));
      p.drawEllipse(multi_btn_draw2);
      p.setBrush(Qt::NoBrush);
      p.drawEllipse(multi_btn_draw3);
      p.drawText(multi_btn_draw1, Qt::AlignCenter, QString("REC"));
      if (s->scene.laneless_mode == 0) {
        p.drawText(QRect(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y-20, m_btn_size, m_btn_size), Qt::AlignCenter, QString("LANE"));
        p.drawText(QRect(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y+20, m_btn_size, m_btn_size), Qt::AlignCenter, QString("LINE"));
      } else if (s->scene.laneless_mode == 1) {
        p.drawText(QRect(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y-20, m_btn_size, m_btn_size), Qt::AlignCenter, QString("LANE"));
        p.drawText(QRect(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y+20, m_btn_size, m_btn_size), Qt::AlignCenter, QString("LESS"));
      } else if (s->scene.laneless_mode == 2) {
        p.drawText(multi_btn_draw2, Qt::AlignCenter, QString("AUTO"));
      }
      p.drawText(multi_btn_draw3, Qt::AlignCenter, QString("TUNE"));
    } else {
      s->scene.multi_btn_slide_timer -= 20;
      s->scene.multi_btn_slide_timer = fmax(s->scene.multi_btn_slide_timer, 0);
      QRect multi_btn_draw1(m_x-(int)s->scene.multi_btn_slide_timer, m_y, m_btn_size, m_btn_size);
      QRect multi_btn_draw2(m_x-(int)s->scene.multi_btn_slide_timer*2, m_y, m_btn_size, m_btn_size);
      QRect multi_btn_draw3(m_x-(int)s->scene.multi_btn_slide_timer*3, m_y, m_btn_size, m_btn_size);
      if (s->scene.multi_btn_slide_timer != 0) {
        p.drawEllipse(multi_btn_draw1);
        p.drawEllipse(multi_btn_draw2);
        p.drawEllipse(multi_btn_draw3);
      }
    }
  }

  // kisapilot standstill
  if (s->scene.standStill && s->scene.comma_stock_ui != 1) {
    int minute = 0;
    int second = 0;
    minute = int(s->scene.standstillElapsedTime / 60);
    second = int(s->scene.standstillElapsedTime) - (minute * 60);
    p.setPen(ochreColor(220));
    debugText(p, surface_rect.right()-UI_BORDER_SIZE-545, UI_BORDER_SIZE+420, "STOP", 220, 135);
    p.setPen(whiteColor(220));
    debugText(p, surface_rect.right()-UI_BORDER_SIZE-545, UI_BORDER_SIZE+550, QString::number(minute).rightJustified(2,'0') + ":" + QString::number(second).rightJustified(2,'0'), 220, 140);
  }

  // kisapilot autohold
  if (s->scene.autoHold && s->scene.comma_stock_ui != 1) {
    int y_pos = 0;
    if (s->scene.steer_warning && (s->scene.car_state.getVEgo() < 0.1 || s->scene.standStill) && s->scene.car_state.getSteeringAngleDeg() < 90) {
      y_pos = 500;
    } else {
      y_pos = 740;
    }
    int width = 500;
    int a_center = s->fb_w/2;
    QRect ah_rect = QRect(a_center - width/2, y_pos, width, 145);
    p.setBrush(blackColor(80));
    p.setPen(QPen(QColor(255, 255, 255, 50), 10));
    p.drawRoundedRect(ah_rect, 20, 20);
    p.setFont(InterFont(79, QFont::Bold));
    p.setPen(greenColor(150));
    p.drawText(ah_rect, Qt::AlignCenter, "AUTO HOLD");
  }

  // kisapilot blinker
  if (s->scene.comma_stock_ui != 1) {
    float bw = 0;
    float bx = 0;
    float bh = 0;
    if (s->scene.leftBlinker) {
      bw = 250;
      bx = s->fb_w/2 - bw/2 - 65;
      bh = s->scene.low_ui_profile?surface_rect.height()/2:200;
      QPointF leftbsign1[] = {{bx, bh-100}, {bx-bw/4, bh-100}, {bx-bw/2, bh}, {bx-bw/4, bh+100}, {bx, bh+100}, {bx-bw/4, bh}};
      bx -= 125;
      QPointF leftbsign2[] = {{bx, bh-100}, {bx-bw/4, bh-100}, {bx-bw/2, bh}, {bx-bw/4, bh+100}, {bx, bh+100}, {bx-bw/4, bh}};
      bx -= 125;
      QPointF leftbsign3[] = {{bx, bh-100}, {bx-bw/4, bh-100}, {bx-bw/2, bh}, {bx-bw/4, bh+100}, {bx, bh+100}, {bx-bw/4, bh}};

      if (s->scene.blinker_blinkingrate<=120 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(70));
        p.drawPolygon(leftbsign1, std::size(leftbsign1));
      }
      if (s->scene.blinker_blinkingrate<=100 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(140));
        p.drawPolygon(leftbsign2, std::size(leftbsign2));
      }
      if (s->scene.blinker_blinkingrate<=80 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(210));
        p.drawPolygon(leftbsign3, std::size(leftbsign3));
      }
    }
    if (s->scene.rightBlinker) {
      bw = 250;
      bx = s->fb_w/2 - bw/2 + bw + 65;
      bh = s->scene.low_ui_profile?surface_rect.height()/2:200;
      QPointF rightbsign1[] = {{bx, bh-100}, {bx+bw/4, bh-100}, {bx+bw/2, bh}, {bx+bw/4, bh+100}, {bx, bh+100}, {bx+bw/4, bh}};
      bx += 125;
      QPointF rightbsign2[] = {{bx, bh-100}, {bx+bw/4, bh-100}, {bx+bw/2, bh}, {bx+bw/4, bh+100}, {bx, bh+100}, {bx+bw/4, bh}};
      bx += 125;
      QPointF rightbsign3[] = {{bx, bh-100}, {bx+bw/4, bh-100}, {bx+bw/2, bh}, {bx+bw/4, bh+100}, {bx, bh+100}, {bx+bw/4, bh}};

      if (s->scene.blinker_blinkingrate<=120 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(70));
        p.drawPolygon(rightbsign1, std::size(rightbsign1));
      }
      if (s->scene.blinker_blinkingrate<=100 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(140));
        p.drawPolygon(rightbsign2, std::size(rightbsign2));
      }
      if (s->scene.blinker_blinkingrate<=80 && s->scene.blinker_blinkingrate>=60) {
        p.setBrush(yellowColor(210));
        p.drawPolygon(rightbsign3, std::size(rightbsign3));
      }
    }
    if (s->scene.leftBlinker || s->scene.rightBlinker) {
      s->scene.blinker_blinkingrate -= 5;
      if(s->scene.blinker_blinkingrate < 0) s->scene.blinker_blinkingrate = 120;
    }
  }

  // kisapilot safetysign
  if (s->scene.comma_stock_ui != 1) {
    int diameter1 = 185;
    int diameter2 = 170;
    int diameter3 = 202;
    int s_center_x = UI_BORDER_SIZE + 305;
    int s_center_y = s->scene.low_ui_profile?(surface_rect.height()-202-35-150+100):(UI_BORDER_SIZE + 100);
    
    int d_center_x = s_center_x;
    int d_center_y = s->scene.low_ui_profile?(s_center_y - 155):(s_center_y + 155);
    int d_width = 220;
    int d_height = 70;
    int opacity = 0;

    QRect rect_s = QRect(s_center_x - diameter1/2, s_center_y - diameter1/2, diameter1, diameter1);
    QRect rect_si = QRect(s_center_x - diameter2/2, s_center_y - diameter2/2, diameter2, diameter2);
    QRect rect_so = QRect(s_center_x - diameter3/2, s_center_y - diameter3/2, diameter3, diameter3);
    QRect rect_d = QRect(d_center_x - d_width/2, d_center_y - d_height/2, d_width, d_height);
    int sl_opacity = 0;
    if (s->scene.sl_decel_off) {
      sl_opacity = 3;
    } else if (s->scene.pause_spdlimit) {
      sl_opacity = 2;
    } else {
      sl_opacity = 1;
    }

    if (s->scene.limitSpeedCamera > 21) {
      if (s->scene.speedlimit_signtype) {
        p.setBrush(whiteColor(255/sl_opacity));
        p.drawRoundedRect(rect_si, 8, 8);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 255/sl_opacity), 12));
        p.drawRoundedRect(rect_s, 8, 8);
        p.setPen(QPen(QColor(255, 255, 255, 255/sl_opacity), 10));
        p.drawRoundedRect(rect_so, 8, 8);
        p.setPen(blackColor(255/sl_opacity));
        debugText(p, rect_so.center().x(), rect_so.center().y()-45, "SPEED", 255/sl_opacity, 36, true);
        debugText(p, rect_so.center().x(), rect_so.center().y()-12, "LIMIT", 255/sl_opacity, 36, true);
        debugText(p, rect_so.center().x(), rect_so.center().y()+UI_BORDER_SIZE+(s->scene.limitSpeedCamera<100?60:50), QString::number(s->scene.limitSpeedCamera), 255/sl_opacity, s->scene.limitSpeedCamera<100?110:90, true);
      } else {
        p.setBrush(whiteColor(255/sl_opacity));
        p.drawEllipse(rect_si);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(redColor(255/sl_opacity), 20));
        p.drawEllipse(rect_s);
        p.setPen(blackColor(255/sl_opacity));
        debugText(p, rect_si.center().x(), rect_si.center().y()+UI_BORDER_SIZE+(s->scene.limitSpeedCamera<100?25:15), QString::number(s->scene.limitSpeedCamera), 255/sl_opacity, s->scene.limitSpeedCamera<100?110:90, true);
      }

      // waze safety type img
      if (s->scene.liveENaviData.ewazealertid == 1) {
        drawIcon(p, s_center_x + 212, s_center_y, waze_cam_img, QColor(0, 0, 0, 0), 1.0);
      } else if (s->scene.liveENaviData.ewazealertid == 2) {
        drawIcon(p, s_center_x + 212, s_center_y, waze_police_img, QColor(0, 0, 0, 0), 1.0);
      }

      if (s->scene.limitSpeedCameraDist != 0) {
        opacity = s->scene.limitSpeedCameraDist>600 ? 0 : (600 - s->scene.limitSpeedCameraDist) * 0.425;
        p.setBrush(redColor(opacity/sl_opacity));
        p.setPen(QPen(QColor(255, 255, 255, 100), 7));
        p.drawRoundedRect(rect_d, 8, 8);
        p.setFont(InterFont(55, QFont::Bold));
        p.setPen(whiteColor(255));
        if (s->scene.is_metric) {
          if (s->scene.limitSpeedCameraDist < 1000) {
            p.drawText(rect_d, Qt::AlignCenter, QString::number(s->scene.limitSpeedCameraDist, 'f', 0) + "m");
          } else if (s->scene.limitSpeedCameraDist < 10000) {
            p.drawText(rect_d, Qt::AlignCenter, QString::number(s->scene.limitSpeedCameraDist/1000, 'f', 2) + "km");
          } else {
            p.drawText(rect_d, Qt::AlignCenter, QString::number(s->scene.limitSpeedCameraDist/1000, 'f', 1) + "km");
          }
        } else {
          if (s->scene.liveENaviData.ewazealertextend) { // waze alert extend
            p.setBrush(orangeColor(150));
            p.drawText(rect_d, Qt::AlignCenter, "Limit");
          } else if ((s->scene.limitSpeedCameraDist*3.28084) < 1000) { // 0m~304m
            p.drawText(rect_d, Qt::AlignCenter, QString::number(s->scene.limitSpeedCameraDist*3.28084, 'f', 0) + "ft");
          } else { // 305m~
            p.drawText(rect_d, Qt::AlignCenter, QString::number(round(s->scene.limitSpeedCameraDist*0.000621*100)/100, 'f', 2) + "mi");
          }
        }
      }
    }
  }

  if (s->scene.live_tune_panel_enable) {
    float bwidth = 160;
    float bheight = 160;
    float x_start_pos_l = s->fb_w/2-bwidth*2;
    float x_start_pos_r = s->fb_w/2+bwidth*2;
    float x_pos = s->fb_w/2;
    float y_pos = 750;
    //upper left arrow
    QPointF leftupar[] = {{x_start_pos_l, y_pos-175}, {x_start_pos_l-bwidth+30, y_pos+bheight/2-175}, {x_start_pos_l, y_pos+bheight-175}};
    p.setBrush(ochreColor(100));
    p.drawPolygon(leftupar, std::size(leftupar));
    //upper right arrow
    QPointF rightupar[] = {{x_start_pos_r, y_pos-175}, {x_start_pos_r+bwidth-30, y_pos+bheight/2-175}, {x_start_pos_r, y_pos+bheight-175}};
    p.setBrush(ochreColor(100));
    p.drawPolygon(rightupar, std::size(rightupar));

    //left arrow
    QPointF leftar[] = {{x_start_pos_l, y_pos}, {x_start_pos_l-bwidth+30, y_pos+bheight/2}, {x_start_pos_l, y_pos+bheight}};
    p.setBrush(greenColor(100));
    p.drawPolygon(leftar, std::size(leftar));
    //right arrow
    QPointF rightar[] = {{x_start_pos_r, y_pos}, {x_start_pos_r+bwidth-30, y_pos+bheight/2}, {x_start_pos_r, y_pos+bheight}};
    p.setBrush(greenColor(100));
    p.drawPolygon(rightar, std::size(rightar));

    int live_tune_panel_list = s->scene.live_tune_panel_list;
    int lateralControlMethod = s->scene.lateralControlMethod;

    QString szTuneName = "";
    QString szTuneParam = "";
    int list_menu = live_tune_panel_list - (s->scene.list_count);
    if (live_tune_panel_list == 0) {
      //szTuneParam.sprintf("%+0.3f", s->scene.cameraOffset*0.001);
      szTuneParam.sprintf("%+0.3f", s->scene.cameraOffset*0.001);
      szTuneName = "CameraOffset";
    } else if (live_tune_panel_list == 1) {
      szTuneParam.sprintf("%+0.3f", s->scene.pathOffset*0.001);
      szTuneName = "PathOffset";
    } else if (lateralControlMethod == 0) {  // 0.PID
      if ( list_menu == 0 ) {
        szTuneParam.sprintf("%0.2f", s->scene.pidKp*0.01);
        szTuneName = "Pid: Kp";
      } else if (list_menu == 1 ) {
        szTuneParam.sprintf("%0.3f", s->scene.pidKi*0.001);
        szTuneName = "Pid: Ki";
      } else if (list_menu == 2 ) {
        szTuneParam.sprintf("%0.2f", s->scene.pidKd*0.01);
        szTuneName = "Pid: Kd";
      } else if (list_menu == 3 ) {
        szTuneParam.sprintf("%0.5f", s->scene.pidKf*0.00001);
        szTuneName = "Pid: Kf";
      }
    } else if (lateralControlMethod == 1) {         // 1.INDI

      if ( list_menu == 0 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiInnerLoopGain*0.1);
        szTuneName = "INDI: ILGain";
      } else if ( list_menu == 1 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiOuterLoopGain*0.1);
        szTuneName = "INDI: OLGain";
      } else if ( list_menu == 2 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiTimeConstant*0.1);
        szTuneName = "INDI: TConst";
      } else if ( list_menu == 3 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiActuatorEffectiveness*0.1);
        szTuneName = "INDI: ActEffct";
      }
    } else if (lateralControlMethod == 2) {       // 2.LQR

      if ( list_menu == 0 ) {
        szTuneParam.sprintf("%0.0f", s->scene.lqrScale*1.0);
        szTuneName = "LQR: Scale";
      } else if ( list_menu == 1) {
        szTuneParam.sprintf("%0.3f", s->scene.lqrKi*0.001);
        szTuneName = "LQR: Ki";
      } else if ( list_menu == 2 ) {
        szTuneParam.sprintf("%0.5f", s->scene.lqrDcGain*0.00001);
        szTuneName = "LQR: DcGain";
      }
    } else if (lateralControlMethod == 3) {     // 3.TORQUE
      if ( list_menu == 0 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKp*0.1);
        szTuneName = "TORQUE: Kp";
      } else if ( list_menu == 1 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKf*0.1);
        szTuneName = "TORQUE: Kf";
      } else if ( list_menu == 2 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKi*0.1);
        szTuneName = "TORQUE: Ki";
      } else if ( list_menu == 3 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueMaxLatAccel*0.1);
        szTuneName = "TORQUE: MaxL";
      } else if ( list_menu == 4 ) {
        szTuneParam.sprintf("%0.3f", s->scene.torqueFriction*0.001);
        szTuneName = "TORQUE: Fric";
      }
    } else if (lateralControlMethod == 4) {     // 4.MULTI
      if ( list_menu == 0 ) {
        szTuneParam.sprintf("%0.2f", s->scene.pidKp*0.01);
        szTuneName = "Pid: Kp";
      } else if (list_menu == 1 ) {
        szTuneParam.sprintf("%0.3f", s->scene.pidKi*0.001);
        szTuneName = "Pid: Ki";
      } else if (list_menu == 2 ) {
        szTuneParam.sprintf("%0.2f", s->scene.pidKd*0.01);
        szTuneName = "Pid: Kd";
      } else if (list_menu == 3 ) {
        szTuneParam.sprintf("%0.5f", s->scene.pidKf*0.00001);
        szTuneName = "Pid: Kf";
      } else if ( list_menu == 4 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiInnerLoopGain*0.1);
        szTuneName = "INDI: ILGain";
      } else if ( list_menu == 5 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiOuterLoopGain*0.1);
        szTuneName = "INDI: OLGain";
      } else if ( list_menu == 6 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiTimeConstant*0.1);
        szTuneName = "INDI: TConst";
      } else if ( list_menu == 7 ) {
        szTuneParam.sprintf("%0.1f", s->scene.indiActuatorEffectiveness*0.1);
        szTuneName = "INDI: ActEffct";
      } else if ( list_menu == 8 ) {
        szTuneParam.sprintf("%0.0f", s->scene.lqrScale*1.0);
        szTuneName = "LQR: Scale";
      } else if ( list_menu == 9) {
        szTuneParam.sprintf("%0.3f", s->scene.lqrKi*0.001);
        szTuneName = "LQR: Ki";
      } else if ( list_menu == 10 ) {
        szTuneParam.sprintf("%0.5f", s->scene.lqrDcGain*0.00001);
        szTuneName = "LQR: DcGain";
      } else if ( list_menu == 11 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKp*0.1);
        szTuneName = "TORQUE: Kp";
      } else if ( list_menu == 12 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKf*0.1);
        szTuneName = "TORQUE: Kf";
      } else if ( list_menu == 13 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueKi*0.1);
        szTuneName = "TORQUE: Ki";
      } else if ( list_menu == 14 ) {
        szTuneParam.sprintf("%0.1f", s->scene.torqueMaxLatAccel*0.1);
        szTuneName = "TORQUE: MaxL";
      } else if ( list_menu == 15 ) {
        szTuneParam.sprintf("%0.3f", s->scene.torqueFriction*0.001);
        szTuneName = "TORQUE: Fric";
      }
    }
    if (szTuneName != "") {
      QRect rect_tune_name = QRect(x_pos-2*bwidth+30+10, y_pos-bheight-20, 4*bwidth-30-10, bheight);
      QRect rect_tune_param = QRect(x_pos-2*bwidth+30+10, y_pos, 4*bwidth-30-10, bheight);
      p.setPen(QColor(255, 255, 255, 255));
      p.setFont(InterFont(80, QFont::Bold));
      p.drawText(rect_tune_name, Qt::AlignCenter, szTuneName);
      p.setFont(InterFont(100, QFont::Bold));
      p.drawText(rect_tune_param, Qt::AlignCenter, szTuneParam);
    }
  }

  // draw rpm arc
  if (s->scene.animated_rpm && !s->scene.low_ui_profile) {
    float max_rpm = (float)s->scene.max_animated_rpm;
    float rpm = (float)fmin((float)s->scene.engine_rpm, max_rpm);
    // float rpm = 1800.0f;
    // yp = y0 + ((y1-y0)/(x1-x0)) * (xp - x0),  yp = interp(xp, [x0, x1], [y0, y1])
    int count = (int)round((18.0f/max_rpm) * rpm); // min:0, max:18
    int arpm_width = 370;
    int arpm_height = 370;
    QRectF rectangle(s->fb_w/2-arpm_width/2, UI_BORDER_SIZE+15, arpm_width, arpm_height);

    if (rpm > 1) {
      p.setFont(InterFont(40, QFont::Normal));
      p.setPen(whiteColor(200));
      p.drawText(QRect(s->fb_w/2-arpm_width/2, UI_BORDER_SIZE+30, arpm_width, arpm_height/4), Qt::AlignCenter, QString::number(rpm, 'f', 0));
      p.setPen(QPen(QBrush(QColor(25, 127, 54, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 0) p.drawArc(rectangle, 225*16, -14*16);
      if (count > 1) p.drawArc(rectangle, 210*16, -14*16);
      if (count > 2) p.drawArc(rectangle, 195*16, -14*16);

      p.setPen(QPen(QBrush(QColor(34, 177, 76, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 3) p.drawArc(rectangle, 180*16, -14*16);
      if (count > 4) p.drawArc(rectangle, 165*16, -14*16);
      if (count > 5) p.drawArc(rectangle, 150*16, -14*16);

      p.setPen(QPen(QBrush(QColor(0, 255, 0, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 6) p.drawArc(rectangle, 135*16, -14*16);
      if (count > 7) p.drawArc(rectangle, 120*16, -14*16);
      if (count > 8) p.drawArc(rectangle, 105*16, -14*16);

      p.setPen(QPen(QBrush(QColor(255, 201, 14, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 9) p.drawArc(rectangle, 90*16, -14*16);
      if (count > 10) p.drawArc(rectangle, 75*16, -14*16);
      if (count > 11) p.drawArc(rectangle, 60*16, -14*16);

      p.setPen(QPen(QBrush(QColor(255, 127, 39, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 12) p.drawArc(rectangle, 45*16, -14*16);
      if (count > 13) p.drawArc(rectangle, 30*16, -14*16);
      if (count > 14) p.drawArc(rectangle, 15*16, -14*16);

      p.setPen(QPen(QBrush(QColor(255, 0, 0, 200)),50,Qt::SolidLine,Qt::FlatCap));
      if (count > 15) p.drawArc(rectangle, 0*16, -14*16);
      if (count > 16) p.drawArc(rectangle, -15*16, -14*16);
      if (count > 17) p.drawArc(rectangle, -30*16, -15*16);
    }
  }

  if (s->scene.cal_view) {
    p.setPen(QPen(Qt::white, 3));
    for (int i = 0; i <= 10; i++) {
      p.drawLine(0, (i*105), s->fb_w, (i*105));
    }
    for (int i = 0; i <= 15; i++) {
      p.drawLine((i*142), 0, (i*142), s->fb_h);
    }
  }

  // date + time + road_name
  if (s->scene.bottom_text_view > 0 && s->scene.comma_stock_ui != 1) {
    int rect_w = 600;
    int rect_x = s->fb_w/2 - rect_w/2;
    const int rect_h = 60;
    const int rect_y = !s->scene.animated_rpm?0:(s->fb_h-rect_h);

    QString road_name = "";
    QString oref = "";
    if (s->scene.navi_select == 1 && s->scene.liveMapData.ocurrentRoadName == "") {
      road_name = QString::fromStdString(s->scene.liveENaviData.ekisaroadname);
    } else if (s->scene.navi_select == 2) {
      road_name = QString::fromStdString(s->scene.liveENaviData.ewazeroadname);
    } else if (s->scene.osm_enabled) {
      road_name = QString::fromStdString(s->scene.liveMapData.ocurrentRoadName);
      oref = QString::fromStdString(s->scene.liveMapData.oref);
    }
    QDateTime now = QDateTime::currentDateTime();
    QString tvalue = "";

    if (s->scene.bottom_text_view == 1) {
      tvalue = now.toString("MM-dd ddd HH:mm:ss");
    } else if (s->scene.bottom_text_view == 2) {
      tvalue = now.toString("MM-dd ddd");
    } else if (s->scene.bottom_text_view == 3) {
      tvalue = now.toString("hh:mm:ss");
    } else if (s->scene.bottom_text_view == 4) {
      tvalue = now.toString("MM-dd ddd HH:mm:ss ") + road_name + oref;
    } else if (s->scene.bottom_text_view == 5) {
      tvalue = now.toString("MM-dd ddd ") + road_name + oref;
    } else if (s->scene.bottom_text_view == 6) {
      tvalue = now.toString("HH:mm:ss ") + road_name + oref;
    } else if (s->scene.bottom_text_view == 7) {
      tvalue = road_name + oref;
    }
    int tw = tvalue.length();
    rect_w = (road_name.length() > 1)?tw*40:tw*33;
    rect_x = s->fb_w/2 - rect_w/2;

    QRect datetime_panel = QRect(rect_x, rect_y, rect_w, rect_h);
    p.setPen(whiteColor(200));
    p.setFont(InterFont(57, QFont::DemiBold));
    p.drawText(datetime_panel, Qt::AlignCenter, tvalue);
    p.setBrush(blackColor(60));
    p.setPen(QPen(blackColor(0), 0));
    p.drawRoundedRect(datetime_panel, 15, 15);
  }

  p.restore();
}

void HudRenderer::drawSetSpeed(QPainter &p, const QRect &surface_rect) {

  UIState *s = uiState();

  // Draw outer box + border to contain set speed
  const QSize default_size = {184, 202};
  QSize set_speed_size = default_size;
  QRect set_speed_rect(QPoint(15 + (default_size.width() - set_speed_size.width()) / 2, s->scene.low_ui_profile?(surface_rect.height()-default_size.height()-35-150):15), set_speed_size);
  if (s->scene.exp_mode_temp) {
    p.setPen(QPen(greenColor(220), 6));
  } else {
    p.setPen(QPen(whiteColor(75), 6));
  }
  if (over_sl) {
    p.setBrush(ochreColor(128));
  } else if (!over_sl && s->scene.limitSpeedCamera > 19){
    p.setBrush(greenColor(100));
  } else if (s->scene.cruiseAccStatus) {
    p.setBrush(blueColor(128));
  } else {
    p.setBrush(blackColor(166));
  }
  p.drawRoundedRect(set_speed_rect, 32, 32);

  // Colors based on status
  QColor max_color = QColor(0xa6, 0xa6, 0xa6, 0xff);
  QColor set_speed_color = QColor(0x72, 0x72, 0x72, 0xff);
  if (is_cruise_set) {
    set_speed_color = QColor(255, 255, 255);
    if (status == STATUS_DISENGAGED) {
      max_color = QColor(255, 255, 255);
    } else if (status == STATUS_OVERRIDE) {
      max_color = QColor(0x91, 0x9b, 0x95, 0xff);
    } else {
      max_color = QColor(0x80, 0xd8, 0xa6, 0xff);
    }
  }

  // Draw "MAX" text and set speed
  QString setSpeedStr = is_cruise_set ? QString::number(std::nearbyint(set_speed)) : "-";
  p.setFont(InterFont(70, QFont::Bold));
  p.setPen(whiteColor(200));
  p.drawText(set_speed_rect.adjusted(0, 10, 0, 0), Qt::AlignTop | Qt::AlignHCenter, s->scene.ctrl_speed > 1?QString::number(s->scene.ctrl_speed, 'f', 0):setSpeedStr);
  p.setPen(QPen(Qt::white, 6));
  p.drawLine(set_speed_rect.left()+35, set_speed_rect.y()+set_speed_size.height()/2-7, set_speed_rect.right()-35, set_speed_rect.y()+set_speed_size.height()/2-7);
  p.setFont(InterFont(90, QFont::Bold));
  p.setPen(set_speed_color);
  if (!s->scene.op_long_enabled) {
    p.drawText(set_speed_rect.adjusted(0, 90, 0, 0), Qt::AlignTop | Qt::AlignHCenter, s->scene.cruiseAccStatus?QString::number(s->scene.vSetDis, 'f', 0):"-");
  } else {
    p.drawText(set_speed_rect.adjusted(0, 90, 0, 0), Qt::AlignTop | Qt::AlignHCenter, s->scene.cruiseAccStatus?setSpeedStr:"-");
  }

  if (s->scene.btn_pressing > 0) {
    p.setPen(QPen(Qt::white, 15));
    p.drawPoint(set_speed_rect.left()+22, set_speed_rect.y()+set_speed_size.height()/2+17);
  }
}

void HudRenderer::drawCurrentSpeed(QPainter &p, const QRect &surface_rect) {

  UIState *s = uiState();

  QString speedStr = QString::number(std::nearbyint(speed));

  float act_accel = (!s->scene.longitudinal_control)?s->scene.a_req_value:s->scene.accel;
  float gas_opacity = act_accel*255>255?255:act_accel*255;
  float brake_opacity = abs(act_accel*175)>255?255:abs(act_accel*175);
  p.setPen(whiteColor(255));
  if (s->scene.brakePress && s->scene.comma_stock_ui != 1) {
  	p.setPen(redColor(255));
  } else if (s->scene.brakeLights && speedStr == "0" && s->scene.comma_stock_ui != 1) {
    p.setPen(QColor(201, 34, 49, 100));
  } else if (s->scene.gasPress && s->scene.comma_stock_ui != 1) {
    p.setPen(QColor(0, 240, 0, 255));
  } else if (act_accel < 0 && act_accel > -5.0 && s->scene.comma_stock_ui != 1) {
    p.setPen(QColor((255-int(abs(act_accel*8))), (255-int(brake_opacity)), (255-int(brake_opacity)), 255));
  } else if (act_accel > 0 && act_accel < 3.0 && s->scene.comma_stock_ui != 1) {
    p.setPen(QColor((255-int(gas_opacity)), (255-int((act_accel*10))), (255-int(gas_opacity)), 255));
  }
  if (!s->scene.low_ui_profile) {
    debugText(p, surface_rect.center().x(), s->scene.animated_rpm?255:210, speedStr, 255, 180, true);
  } else {
    p.setFont(InterFont(180, QFont::Bold));
    uiText(p, surface_rect.left()+20, surface_rect.height()-25, speedStr, 255, true);
  }
  if (!s->scene.low_ui_profile) {
    if (s->scene.brakeLights) {
      p.setPen(redColor(200));
    } else {
      p.setPen(whiteColor(255));
    }
    debugText(p, surface_rect.center().x(), s->scene.animated_rpm?315:280, is_metric ? tr("KPH") : tr("MPH"), 255, 50, true);
  } else {
    if (s->scene.brakeLights) {
      p.setPen(QPen(Qt::red, 15));
      p.drawPoint(UI_BORDER_SIZE+1, surface_rect.height()-UI_BORDER_SIZE-1);
    } else {
      p.setPen(whiteColor(255));
    }
  }
}

void HudRenderer::drawText(QPainter &p, int x, int y, const QString &text, int alpha) {
  QRect real_rect = p.fontMetrics().boundingRect(text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});

  p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void HudRenderer::drawIcon(QPainter &p, int x, int y, QPixmap &img, QBrush bg, float opacity, bool rotation, float angle) {
  // kisapilot
  const int btn_size = 192;
  if (rotation) {
    p.setOpacity(1.0);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
    p.setOpacity(opacity);
    p.save();
    p.translate(x, y);
    p.rotate(-angle);
    //img.scaled(105,105);
    QRect r = img.rect();
    r.moveCenter(QPoint(0,0));
    p.drawPixmap(r, img);
    p.setOpacity(1.0);
    p.restore();
  } else {
    p.setOpacity(1.0);  // bg dictates opacity of ellipse
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(x - btn_size / 2, y - btn_size / 2, btn_size, btn_size);
    p.setOpacity(opacity);
    p.drawPixmap(x - img.size().width() / 2, y - img.size().height() / 2, img);
    p.setOpacity(1.0);
  }
}

void HudRenderer::uiText(QPainter &p, int x, int y, const QString &text, int alpha, bool custom_color) {
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x + real_rect.width() / 2, y - real_rect.height() / 2});

  if (!custom_color) {
    p.setPen(QColor(0xff, 0xff, 0xff, alpha));
  }
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void HudRenderer::debugText(QPainter &p, int x, int y, const QString &text, int alpha, int fontsize, bool bold) {
  if (bold) {
  	p.setFont(InterFont(fontsize, QFont::Bold));
  } else {
  	p.setFont(InterFont(fontsize, QFont::DemiBold));
  }
  QFontMetrics fm(p.font());
  QRect init_rect = fm.boundingRect(text);
  QRect real_rect = fm.boundingRect(init_rect, 0, text);
  real_rect.moveCenter({x, y - real_rect.height() / 2});
  p.drawText(real_rect.x(), real_rect.bottom(), text);
}

void HudRenderer::drawWheelState(QPainter &p, const QRect &surface_rect) {

  UIState *s = uiState();

  p.save();

  const int btn_size = 192;
  if (s->scene.enabled) {
    drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size/2 - 10):(btn_size/2 + 10),
     s->scene.experimental_mode?experimental_img:engage_img, QColor(23, 134, 68, 150), 1.0, true, s->scene.angleSteers);
  } else if (!s->scene.comma_stock_ui) {
    QString gear_text = "0";
    switch(int(s->scene.getGearShifter)) {
      case 1 : drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size / 2 - 10):(btn_size / 2 + 10), gear_img_p); break;
      case 2 : drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size / 2 - 10):(btn_size / 2 + 10), gear_img_d); break;
      case 3 : drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size / 2 - 10):(btn_size / 2 + 10), gear_img_n); break;
      case 4 : drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size / 2 - 10):(btn_size / 2 + 10), gear_img_r); break;
      case 5 : gear_text = "M"; p.setPen(greenColor(255)); debugText(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size/2 + 70):(btn_size / 2 + 70 + 10), gear_text, 255, 190, true); break;
      case 7 : gear_text = "B"; p.setPen(whiteColor(255)); debugText(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size/2 + 70):(btn_size / 2 + 70 + 10), gear_text, 255, 190, true); break;
      default: gear_text = QString::number(int(s->scene.getGearShifter), 'f', 0); p.setPen(whiteColor(255)); debugText(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size/2 + 70):(btn_size / 2 + 70 + 10), gear_text, 255, 190, true); break;
    }
  } else {
    drawIcon(p, surface_rect.right() - btn_size / 2 - 10, s->scene.low_ui_profile?(surface_rect.height() - btn_size / 2 - 10):(btn_size / 2 + 10), engage_img, blackColor(100), 0.9);
  }

  p.restore();
}