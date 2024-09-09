package polimi.server.supervisors;

import akka.actor.*;
import akka.japi.pf.DeciderBuilder;
import akka.pattern.Patterns;
import akka.util.Timeout;

import polimi.server.actors.TopicManager;
import polimi.server.messages.AskForActorMap;
import polimi.server.messages.StartOperators;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scala.concurrent.Await;
import scala.concurrent.Future;

import java.time.Duration;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

public class TopicSupervisor extends AbstractActor implements Supervisor{

    private static final SupervisorStrategy strategy =
            new OneForOneStrategy(
                    1, // Max no of retries
                    Duration.ofMinutes(1), // Within what time period
                    DeciderBuilder.match(Exception.class, e -> SupervisorStrategy.restart())
                            .build());

    private final Logger log = LoggerFactory.getLogger(TopicSupervisor.class);

    private final Timeout timeout = new Timeout(scala.concurrent.duration.Duration.create(5, TimeUnit.SECONDS));

    private Map<String, ActorRef> topicManagerMap;

    public TopicSupervisor(String[] topics) throws InterruptedException, TimeoutException {
        topicManagerMap = new HashMap<>();
        for (String topic : topics) {
            ActorRef topicManager = getContext().actorOf(TopicManager.props(topic), topic+"-manager");
            topicManagerMap.put(topic, topicManager);
            Future<Object> future = Patterns.ask(topicManager, new StartOperators(topic), timeout);
            Await.result(future, timeout.duration());
        }
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(Props.class, this::handleProps)
                .match(AskForActorMap.class, this::sendActorMap)
                .build();
    }

    private void handleProps(Props props) {
        if (props.clazz().equals(OperatorSupervisor.class)) {
            ActorRef opSup = getContext().actorOf(props, sender().
                    path().name().split("-")[0] + "-operator-supervisor");
            log.info("Operator supervisor created at: {}", opSup.path());
        } else log.error("Cannot handle class: {}", props.clazz());
    }

    private void sendActorMap(AskForActorMap msg) {
        sender().tell(topicManagerMap, self());
    }

    @Override
    public SupervisorStrategy supervisorStrategy() {
        return strategy;
    }

    public static Props props(String[] topics) {
        return Props.create(TopicSupervisor.class, (Object) topics); }
}
