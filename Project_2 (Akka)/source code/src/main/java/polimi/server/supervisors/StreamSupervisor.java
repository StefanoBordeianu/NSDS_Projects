package polimi.server.supervisors;

import akka.actor.AbstractActor;
import akka.actor.OneForOneStrategy;
import akka.actor.Props;
import akka.actor.SupervisorStrategy;
import akka.japi.pf.DeciderBuilder;

import polimi.server.actors.StreamManager;

import java.time.Duration;

public class StreamSupervisor extends AbstractActor implements Supervisor{

    private static final SupervisorStrategy strategy =
            new OneForOneStrategy(
                    1, // Max no of retries
                    Duration.ofMinutes(1), // Within what time period
                    DeciderBuilder.match(Exception.class, e -> SupervisorStrategy.restart())
                            .build());

    @Override
    public SupervisorStrategy supervisorStrategy() {
        return strategy;
    }

    public StreamSupervisor() {
    }

    @Override
    public AbstractActor.Receive createReceive() {
        // Creates the child actor within the supervisor actor context
        return receiveBuilder()
                .match(
                        Props.class,
                        props -> {
                            if (props.clazz().equals(StreamManager.class)) {
                                getSender().tell(getContext().actorOf(props, "stream-manager"), getSelf());
                            } else if (props.clazz().equals(TopicSupervisor.class)) {
                                getSender().tell(getContext().actorOf(props, "topic-supervisor"), getSelf());
                            }
                        })
                .build();
    }

    public static Props props() {
        return Props.create(StreamSupervisor.class);
    }
}
