package polimi.server.supervisors;

import akka.actor.*;
import akka.japi.pf.DeciderBuilder;
import com.typesafe.config.Config;
import polimi.server.actors.Operator;
import polimi.server.messages.AskForActorMap;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

public class OperatorSupervisor extends AbstractActor implements Supervisor {

    private static final SupervisorStrategy strategy = new OneForOneStrategy(1, // Max no of retries
            Duration.ofMinutes(1), // Within what time period
            DeciderBuilder.match(Exception.class, e -> SupervisorStrategy.restart())
                    .build());

    private final Logger log = LoggerFactory.getLogger(OperatorSupervisor.class);

    private final String topic;
    private int activeOperators = 0;
    List<ActorRef> ops;

    @Override
    public SupervisorStrategy supervisorStrategy() {
        return strategy;
    }

    public OperatorSupervisor(String topic){
        this.topic = topic;
        Config config = getContext().getSystem().settings().config();
        ops = new ArrayList<>();
        for (int i = 0; i < config.getInt("stream-processing.num-operators"); i++) {
            ActorRef op = getContext().actorOf(Operator.props(topic, config), topic+"-operator-"+i);
            ops.add(op);
            log.info("Operator created for topic {} at {}: ", topic, op.path());
        }
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(
                        Props.class,
                        props -> {
                            activeOperators++;
                            getSender().tell(getContext().actorOf(props, topic+"operator"+activeOperators), getSelf());
                        })
                .match(AskForActorMap.class, this::sendActorList)
                .build();
    }

    private void sendActorList(AskForActorMap msg) {
        sender().tell(ops, self());
    }

    public static Props props(String topic){ return Props.create(OperatorSupervisor.class, topic); }

}
