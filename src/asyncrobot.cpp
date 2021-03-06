#include "asyncrobot.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QTimer>

AsyncRobot::AsyncRobot() 
{
  jointSignalEnable_ = false;
  accelSignalEnable_ = false;
  timer_ = new QTimer(this);
}

AsyncRobot::~AsyncRobot() {}

void AsyncRobot::bindMobot(CLinkbot* mobot)
{
  mobotLock_.lock();
  mobot_ = mobot;
  anglesDirtyMask_ = 0;
  mobotLock_.unlock();
}

void AsyncRobot::enableJointSignals(bool enable)
{
  qDebug() << "enable Joint Signals: " << enable;
  mobotLock_.lock();
  jointSignalEnable_ = enable;
  mobotLock_.unlock();
}

void AsyncRobot::enableAccelSignals(bool enable)
{
  mobotLock_.lock();
  accelSignalEnable_ = enable;
  mobotLock_.unlock();
}

void AsyncRobot::disableJointSignals()
{
  qDebug() << "Joint Signals Disabled.";
  enableJointSignals(false);
}

void AsyncRobot::disableAccelSignals()
{
  enableAccelSignals(false);
}

void AsyncRobot::acquireJointControl()
{
  qDebug() << "Joint control acquired.";
  jointControl_ = true;
}

void AsyncRobot::releaseJointControl()
{
  qDebug() << "Joint control released.";
  jointControl_ = false;
}

void AsyncRobot::stopWork()
{
  mobotLock_.lock();
  timer_->stop();
  mobotLock_.unlock();
}

void AsyncRobot::driveJointTo(int joint, double angle)
{
  if(false == jointControl_) return;
  qDebug() << "Drive joint " << joint << " to " << angle;
  desiredJointAnglesLock_.lock();
  desiredJointAngles_[joint-1] = angle;
  anglesDirtyMask_ |= 1<<(joint-1);
  desiredJointAnglesLock_.unlock();
}

void AsyncRobot::doWork()
{
  double jointAngles[3];
  double accel[3];
  mobotLock_.lock();
  if(
      (mobot_ == NULL) ||
      (!mobot_->isConnected())
    ) {
    mobotLock_.unlock();
    return;
  }
  if(jointSignalEnable_) {
    mobot_->getJointAngles( 
        jointAngles[0], 
        jointAngles[1], 
        jointAngles[2]);
    if(memcmp(jointAngles, lastJointAngles_, sizeof(double)*3)) {
      emit jointAnglesChanged(jointAngles[0], jointAngles[1], jointAngles[2]);
      double norm;
      if(jointAngles[0] != lastJointAngles_[0]) {
        emit joint1Changed(((int)-jointAngles[0])%360);
        norm = fmod(jointAngles[0], 360);
        if(norm > 180.0) {
          norm -= 360.0;
        }
        emit joint1Changed(norm);
      }
      if(jointAngles[1] != lastJointAngles_[1]) {
        /* Negative angles because the directionality of the Qt dials is
         * opposite of our joint directions. */
        emit joint2Changed(((int)-jointAngles[1])%360);
        norm = fmod(jointAngles[1], 360);
        if(norm > 180.0) {
          norm -= 360.0;
        }
        emit joint2Changed(norm);
      }
      if(jointAngles[2] != lastJointAngles_[2]) {
        emit joint2Changed(((int)-jointAngles[2])%360);
        norm = fmod(jointAngles[1], 360);
        if(norm > 180.0) {
          norm -= 360.0;
        }
        emit joint2Changed(norm);
      }
      memcpy(lastJointAngles_, jointAngles, sizeof(double)*3);
    }
  }
  if(accelSignalEnable_) {
    mobot_->getAccelerometerData(accel[0], accel[1], accel[2]);
    if(memcmp(accel, lastAccel_, sizeof(double)*3)) {
      emit accelChanged(accel[0], accel[1], accel[2]);
      memcpy(lastAccel_, accel, sizeof(double)*3);
    }
  }

  desiredJointAnglesLock_.lock();
  int i;
  for(i = 0; i < 4; i++) {
    if(anglesDirtyMask_ & (1<<i)) {
      mobot_->driveJointToDirectNB( (robotJointId_t)(i+1), desiredJointAngles_[i]);
    }
  }
  anglesDirtyMask_ = 0;
  desiredJointAnglesLock_.unlock();

  mobotLock_.unlock();
  QThread::yieldCurrentThread();
}

void AsyncRobot::setState(int state)
{
  if(state) {
    enableJointSignals(true);
    enableAccelSignals(true);
  } else {
    enableJointSignals(false);
    enableAccelSignals(false);
  }
}

void AsyncRobot::startWork()
{
  mobotLock_.lock();
  if(timer_->isActive()) {
    mobotLock_.unlock();
    return;
  }
  anglesDirtyMask_ = 0;
  connect(timer_, SIGNAL(timeout()), this, SLOT(doWork()));
  timer_->start();
  mobotLock_.unlock();
}

