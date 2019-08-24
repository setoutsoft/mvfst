/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <quic/server/state/ServerStateMachine.h>
#include <quic/state/QuicStreamManager.h>
#include <quic/state/test/Mocks.h>

using namespace testing;

namespace quic {
namespace test {

class QuicStreamManagerTest : public Test {
 public:
  void SetUp() override {
    conn.flowControlState.peerAdvertisedInitialMaxStreamOffsetBidiLocal =
        kDefaultStreamWindowSize;
    conn.flowControlState.peerAdvertisedInitialMaxStreamOffsetBidiRemote =
        kDefaultStreamWindowSize;
    conn.flowControlState.peerAdvertisedInitialMaxStreamOffsetUni =
        kDefaultStreamWindowSize;
    conn.flowControlState.peerAdvertisedMaxOffset =
        kDefaultConnectionWindowSize;
    conn.streamManager->setMaxLocalBidirectionalStreams(
        kDefaultMaxStreamsBidirectional);
    conn.streamManager->setMaxLocalUnidirectionalStreams(
        kDefaultMaxStreamsUnidirectional);
    auto congestionController =
        std::make_unique<NiceMock<MockCongestionController>>();
    mockController = congestionController.get();
    conn.congestionController = std::move(congestionController);
  }

  QuicServerConnectionState conn;
  MockCongestionController* mockController;
};

TEST_F(QuicStreamManagerTest, TestAppIdleCreateBidiStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());

  // The app limiited state did not change.
  EXPECT_CALL(*mockController, setAppIdle(false, _)).Times(0);
  auto stream = manager.createNextBidirectionalStream();
  StreamId id = stream.value()->id;
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  // Force transition to closed state
  stream.value()->send.state = StreamSendStates::Closed();
  stream.value()->recv.state = StreamReceiveStates::Closed();
  manager.removeClosedStream(stream.value()->id);
  EXPECT_TRUE(manager.isAppIdle());
  EXPECT_EQ(manager.getStream(id), nullptr);
}

TEST_F(QuicStreamManagerTest, TestAppIdleCreateUnidiStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  EXPECT_CALL(*mockController, setAppIdle(false, _)).Times(0);
  auto stream = manager.createNextUnidirectionalStream();
  EXPECT_FALSE(manager.isAppIdle());

  // Force transition to closed state
  EXPECT_CALL(*mockController, setAppIdle(true, _));
  stream.value()->send.state = StreamSendStates::Closed();
  stream.value()->recv.state = StreamReceiveStates::Closed();
  manager.removeClosedStream(stream.value()->id);
  EXPECT_TRUE(manager.isAppIdle());
}

TEST_F(QuicStreamManagerTest, TestAppIdleExistingLocalStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  EXPECT_CALL(*mockController, setAppIdle(false, _)).Times(0);

  auto stream = manager.createNextUnidirectionalStream();
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  manager.setStreamAsControl(*stream.value());
  EXPECT_TRUE(manager.isAppIdle());

  manager.getStream(stream.value()->id);
  EXPECT_TRUE(manager.isAppIdle());
}

TEST_F(QuicStreamManagerTest, TestAppIdleStreamAsControl) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());

  auto stream = manager.createNextUnidirectionalStream();
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  manager.setStreamAsControl(*stream.value());
  EXPECT_TRUE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(false, _));
  manager.createNextUnidirectionalStream();
  EXPECT_FALSE(manager.isAppIdle());
}

TEST_F(QuicStreamManagerTest, TestAppIdleCreatePeerStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  StreamId id = 0;
  auto stream = manager.getStream(id);
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  manager.setStreamAsControl(*stream);
  EXPECT_TRUE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(false, _));
  StreamId id2 = 4;
  manager.getStream(id2);
  EXPECT_FALSE(manager.isAppIdle());
}

TEST_F(QuicStreamManagerTest, TestAppIdleExistingPeerStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  EXPECT_CALL(*mockController, setAppIdle(false, _)).Times(0);

  StreamId id = 0;
  auto stream = manager.getStream(id);
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  manager.setStreamAsControl(*stream);
  EXPECT_TRUE(manager.isAppIdle());

  manager.getStream(id);
  EXPECT_TRUE(manager.isAppIdle());
}

TEST_F(QuicStreamManagerTest, TestAppIdleClosePeerStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  StreamId id = 0;
  auto stream = manager.getStream(id);
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  // Force transition to closed state
  stream->send.state = StreamSendStates::Closed();
  stream->recv.state = StreamReceiveStates::Closed();
  manager.removeClosedStream(stream->id);
  EXPECT_TRUE(manager.isAppIdle());
  EXPECT_EQ(manager.getStream(id), nullptr);
}

TEST_F(QuicStreamManagerTest, TestAppIdleCloseControlStream) {
  auto& manager = *conn.streamManager;
  EXPECT_FALSE(manager.isAppIdle());
  EXPECT_CALL(*mockController, setAppIdle(false, _)).Times(0);

  StreamId id = 0;
  auto stream = manager.getStream(id);
  EXPECT_FALSE(manager.isAppIdle());

  EXPECT_CALL(*mockController, setAppIdle(true, _));
  manager.setStreamAsControl(*stream);
  EXPECT_TRUE(manager.isAppIdle());

  // Force transition to closed state
  stream->send.state = StreamSendStates::Closed();
  stream->recv.state = StreamReceiveStates::Closed();
  manager.removeClosedStream(stream->id);
  EXPECT_TRUE(manager.isAppIdle());
}
} // namespace test
} // namespace quic
