// Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause-Clear

#include "qrb_ros_motion_service/motion_node/line_motion_action.hpp"

using namespace std::placeholders;

namespace qrb_ros
{
namespace motion_service
{

LineMotionAction::LineMotionAction(std::shared_ptr<rclcpp::Node> node_handler,
                                   std::shared_ptr<MotionServiceProxy> service_proxy)
{
  RCLCPP_INFO(logger_, "create");
  node_handler_ = node_handler;
  motion_service_proxy_ = service_proxy;
  create_motion_action_server();
}

LineMotionAction::~LineMotionAction()
{
  RCLCPP_INFO(logger_, "destroy");
}

void LineMotionAction::create_motion_action_server()
{
  RCLCPP_INFO(logger_, "create_motion_action_server");
  if (node_handler_ != nullptr) {
    servers_ptr_ = rclcpp_action::create_server<LineMotion>(
        node_handler_->get_node_base_interface(), node_handler_->get_node_clock_interface(),
        node_handler_->get_node_logging_interface(), node_handler_->get_node_waitables_interface(),
        "line_motion", std::bind(&LineMotionAction::handle_goal, this, _1, _2),
        std::bind(&LineMotionAction::handle_cancel, this, _1),
        std::bind(&LineMotionAction::handle_accepted, this, _1));

    // register command callback
    command_callback_ =
      [&](int request_id, bool result) {
      receive_command_callback(request_id, result);
    };
    if (motion_service_proxy_ != nullptr) {
      motion_service_proxy_->register_command_callback(
        command_callback_, MOTION_COMMAND_TYPE::LINE_MOTION_COMMND);
    }
  }
}

// ====================== Arc Motion ===========================
rclcpp_action::GoalResponse LineMotionAction::handle_goal(
  const rclcpp_action::GoalUUID & uuid,
  std::shared_ptr<const LineMotion::Goal> goal)
{
  RCLCPP_INFO(logger_, "received goal request");
  (void)uuid;
  qrb_ros_motion_msgs::msg::VelocityLevel velocity = goal->velocity;
  double distance = goal->distance;
  if (motion_service_proxy_ != nullptr) {
    int request_id = motion_service_proxy_->request_line_motion(velocity, distance);
    if (request_id > 0) {
      RCLCPP_INFO(logger_, "request line motion sucessfully, request_id: %d", request_id);
      request_id_map_.insert(std::pair<rclcpp_action::GoalUUID, int>(uuid, request_id));
      return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
    }
  }
  return rclcpp_action::GoalResponse::REJECT;
}

rclcpp_action::CancelResponse LineMotionAction::handle_cancel(
  const std::shared_ptr<GoalHandleLineMotion> goal_handle)
{
  RCLCPP_INFO(logger_, "received request to cancel goal");
  (void)goal_handle;
  rclcpp_action::GoalUUID goal_id = goal_handle->get_goal_id();
  if (request_id_map_.count(goal_id) <= 0) {
    RCLCPP_ERROR(logger_, "can not find request id from goal_handle");
    return rclcpp_action::CancelResponse::REJECT;
  }
  int request_id = request_id_map_[goal_id];
  if (motion_service_proxy_ != nullptr) {
    if(motion_service_proxy_->request_stop_motion(request_id)) {
      return rclcpp_action::CancelResponse::ACCEPT;
    }
  }
  return rclcpp_action::CancelResponse::REJECT;
}

void LineMotionAction::handle_accepted(const std::shared_ptr<GoalHandleLineMotion> goal_handle)
{
  RCLCPP_INFO(logger_, "handle accepted");
  rclcpp_action::GoalUUID goal_id = goal_handle->get_goal_id();
  if (request_id_map_.count(goal_id) <= 0) {
    RCLCPP_ERROR(logger_, "can not find request id from goal_handle");
    return;
  }
  int request_id = request_id_map_[goal_id];
  handle_map_.insert(
      std::pair<int, std::shared_ptr<GoalHandleLineMotion>>(request_id, goal_handle));
}

void LineMotionAction::receive_command_callback(int request_id, bool result)
{
  RCLCPP_INFO(logger_, "receive_command_callback");
  if (!rclcpp::ok()) {
    return;
  }
  if (handle_map_.count(request_id) <= 0) {
    return;
  }

  std::shared_ptr<GoalHandleLineMotion> handle = handle_map_[request_id];
  if (handle == nullptr) {
    RCLCPP_INFO(logger_, "server global handle is nullptr");
    return;
  }

  auto action_result = std::make_shared<LineMotion::Result>();
  action_result->result = result;
  if (handle->is_canceling()) {
    handle->canceled(action_result);
    RCLCPP_INFO(logger_, "Goal canceled");
  } else {
    handle->succeed(action_result);
    RCLCPP_INFO(logger_, "Goal succeeded");
  }

  request_id_map_.erase(handle->get_goal_id());
  handle_map_.erase(request_id);
  handle = nullptr;
}

}  // namespace motion_service
}  // namespace qrb_ros
