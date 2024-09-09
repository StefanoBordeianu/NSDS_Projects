package polimi.server.actors;

import akka.actor.*;
import akka.japi.pf.ReceiveBuilder;
import akka.pattern.Patterns;
import akka.util.Timeout;
import com.typesafe.config.Config;
import polimi.server.messages.*;
import polimi.server.messages.*;
import polimi.server.messages.*;
import polimi.server.supervisors.TopicSupervisor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scala.concurrent.Await;
import scala.concurrent.Future;
import scala.concurrent.duration.Duration;

import java.util.HashMap;
import java.util.Map;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

public class StreamManager extends AbstractActor {

    private final Config config;
    private final Logger log = LoggerFactory.getLogger(StreamManager.class);
    private Map<String, ActorRef> topicManagerMap;
    private final Timeout timeout = new Timeout(Duration.create(5, TimeUnit.SECONDS));

    public StreamManager(){
        this.config = getContext().getSystem().settings().config();
        this.topicManagerMap = new HashMap<>();
        log.info("StreamManager.class || Initialization Complete");
        log.info("StreamManager.class || Stream manager initialized at: " + self().path());
        self().tell(new StartManagers(this.config), self());
    }

    @Override
    public Receive createReceive() {
        return ReceiveBuilder.create()
                .match(StartManagers.class, msg -> {
                    log.info("StreamManager.class || StartManagers.start");
                    startManagers(msg);
                })
                .match(String.class, log::info)
                .match(SensorData.class, this::sendToTopicManager)
                .match(ComputationComplete.class, this::Terminate)
                .match(Terminated.class, msg -> getContext().system().terminate())
                .match(WatchMe.class, this::handleWatchMe)
                .build();
    }

    private void handleWatchMe(WatchMe watchMe) {
        getContext().watch(watchMe.getActorToWatch());
    }

    private void startManagers(StartManagers message) throws InterruptedException, TimeoutException {
        String[] keys = message.getKeys().toArray(new String[0]);
        Future<Object> waitForTopicSupervisor = Patterns.ask(getContext().parent(), TopicSupervisor.props(keys), timeout);
        ActorRef topicSupervisor = (ActorRef) Await.result(waitForTopicSupervisor, Duration.create(5, TimeUnit.SECONDS));
        Future<Object> waitForTopicMap = Patterns.ask(topicSupervisor, new AskForActorMap(), timeout);
        topicManagerMap = (Map<String, ActorRef>) Await.result(waitForTopicMap, Duration.create(5, TimeUnit.SECONDS));
        sender().tell("Managers started", self());
    }

    private void sendToTopicManager(SensorData message) throws Exception {

        if (message.getOp() == 0) {
            topicManagerMap.get(message.getKey()).tell(message, self());
        } else if (message.getOp() == 1) {
            throw new Exception("manager fault");
        }

    }

    public static Props props() {
        return Props.create(StreamManager.class);
    }

    private void Terminate(ComputationComplete msg){
        if (topicManagerMap.isEmpty()) {
            log.info("StreamManager.class || System termination");
            getContext().system().terminate();
        }
    }
}
