package polimi.server.actors;

import akka.actor.*;
import akka.pattern.Patterns;
import akka.util.Timeout;

import com.typesafe.config.Config;
import polimi.server.messages.*;
import polimi.server.messages.*;
import polimi.server.messages.*;
import polimi.server.supervisors.OperatorSupervisor;
import polimi.server.utils.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scala.concurrent.Await;
import scala.concurrent.ExecutionContextExecutor;
import scala.concurrent.Future;
import scala.concurrent.duration.Duration;

import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;

public class TopicManager extends AbstractActor {
    private List<ActorRef> operatorList;
    private final Map<Long, ActorRef> senderMap; // Track sender for retries
    private final Logger log = LoggerFactory.getLogger(TopicManager.class);
    private final Config config;
    private final AtomicInteger roundRobinCounter = new AtomicInteger(0);
    private final ExecutionContextExecutor dispatcher = getContext().getDispatcher();
    private String topic;
    private final Timeout timeout = new Timeout(scala.concurrent.duration.Duration.create(5, TimeUnit.SECONDS));


    public TopicManager(String topic) {
        log.info("Creating {} manager", topic);
        this.topic = topic;
        this.config = getContext().getSystem().settings().config();
        operatorList = new ArrayList<>();
        this.senderMap = new HashMap<>();
        log.info("Manager created at {}", self().path());
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(SensorData.class, this::routeData)
                .match(AggregatedData.class, this::storeAggregatedData)
                .match(ComputationComplete.class, msg -> {
                    sender().tell("acknowledged", self());
                    log.info("End of computation");
                })
                .match(StartOperators.class, this::handleStartOperators)
                .match(InitializationComplete.class, this::handleInitializationComplete)
                .match(DeliveryFailed.class, this::handleDeliveryFailure)
                .build();
    }

    private void handleStartOperators(StartOperators msg) throws InterruptedException, TimeoutException {
        getContext().getParent().tell(OperatorSupervisor.props(msg.getKey()), self());
        sender().tell("acknowledged", self());
        String opSupPath = getContext().getParent().path() + "/" + this.topic + "-operator-supervisor";
        ActorSelection opSup = getContext().getSystem().actorSelection(opSupPath);
        Future<Object> future = Patterns.ask(opSup, new AskForActorMap(), timeout);
        operatorList = (List<ActorRef>) Await.result(future, timeout.duration());
        log.info("Operators started");
    }

    private void storeAggregatedData(AggregatedData aggregatedData) {
        getContext().system().actorSelection("user/stream-writer").tell(aggregatedData, self());
    }

    private void routeData(SensorData msg) throws Exception {
        String keyPartition = msg.getKey();
        if (msg.getOp() == 0) {
            if (!operatorList.isEmpty()) {
                long sequenceNumber = msg.getSequenceNumber();
                senderMap.put(sequenceNumber, sender()); // Track sender for retries

                // Load balance using round-robin
                int index = roundRobinCounter.getAndIncrement() % operatorList.size();
                operatorList.get(index).tell(msg, self());
            } else {
                log.warn("{} Manager || No operator found for key partition: {}",
                        StringUtils.capitalizeWords(msg.getKey()),
                        keyPartition);
            }
        } else if (msg.getOp() == 2) {
            throw new Exception("Stream Manager fault");
        }
    }

    private void handleDeliveryFailure(DeliveryFailed failedMessage) {
        if (failedMessage.getMessage() instanceof SensorData failedMsg) {
            long sequenceNumber = failedMsg.getSequenceNumber();
            ActorRef originalSender = senderMap.get(sequenceNumber);

            if (originalSender != null) {
                log.info("Retrying message delivery for sequence number: {}", sequenceNumber);

                // Retry sending the message
                Future<Object> future = Patterns.ask(originalSender, failedMsg, Timeout.apply(Duration.create(3, TimeUnit.SECONDS)));
                Patterns.pipe(future, dispatcher).to(self());
            } else {
                log.warn("No original sender found for sequence number: {}", sequenceNumber);
            }
        } else {
            log.warn("Unexpected delivery failure for message: {}", failedMessage.getMessage());
        }
    }

    private void handleInitializationComplete(InitializationComplete msg){
        log.info("TopicManager.class || {} manager initialized", msg.getKey());
        self().tell(new StartOperators(msg.getKey()), self());
        sender().tell("Initialization complete", self());
    }

    public static Props props(String topic) {
        return Props
                .create(TopicManager.class, topic);
    }

}
