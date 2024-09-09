package polimi.server.actors;

import polimi.server.messages.AggregatedData;
import polimi.server.messages.DeliveryFailed;
import polimi.server.messages.SensorData;
import polimi.server.utils.SequenceNumberGenerator;
import polimi.server.utils.Window;

import akka.actor.AbstractActor;
import akka.actor.ActorSelection;
import akka.actor.Props;
import akka.actor.Terminated;
import com.typesafe.config.Config;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;
import java.util.function.Function;

public class Operator extends AbstractActor {
    private static final Logger log = LoggerFactory.getLogger(Operator.class);

    private final String keyPartition;
    private final Window window;
    private final Function<List<Double>, Double> aggregateFunction;
    private final Config config;
    private final String aggregationType;
    private final long sequenceNumber;

    public Operator(String keyPartition, Function<List<Double>, Double> aggregateFunction) {
        this.config = getContext().getSystem().settings().config();
        this.keyPartition = keyPartition;
        this.window = new Window(config.getInt("stream-processing.operators." + keyPartition + ".window-size"),
                config.getInt("stream-processing.operators." + keyPartition + ".slide"));
        this.aggregateFunction = aggregateFunction;
        this.aggregationType = config.getString("stream-processing.operators." + keyPartition + ".aggregation-function");
        this.sequenceNumber = SequenceNumberGenerator.generateSequenceNumber();
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(SensorData.class, this::processData)
                .match(Terminated.class, msg -> context().stop(self()))
                .match(DeliveryFailed.class, this::deliveryFailed)
                .build();
    }

    private void deliveryFailed(DeliveryFailed msg) throws Exception {
        log.info("Delivery failed: {}", msg);
        throw new Exception("Delivery failed");
    }

    private void processData(SensorData msg) throws Exception {
        log.info("Processing data: {}", msg);
        if (!verifyCorrectPartition(msg.getKey())) {
            forwardToCorrectPartition(msg);
            return;
        }

        if (msg.getOp() == 0) {
            try {
                // Insert the value into the window
                window.insert(msg.getValue());

                // If the window is full, aggregate and forward the data
                if (window.isFull()) {
                    aggregateAndForward();
                    window.slide(window.getSlide());
                }
            } catch (Exception e) {
                log.error("Error processing data: {}", msg, e);
            }
        } else if (msg.getOp() == 3){
            log.error("Operator fault!");
            throw new Exception("Operator Fault");
        }
    }

    private void forwardToCorrectPartition(SensorData msg) {
        String targetPath = getContext().getParent().path() + "/" + msg.getKey() + "-operator-0";
        ActorSelection targetSelection = context().system().actorSelection(targetPath);
        log.info("Forwarding message to: {}", targetPath);
        targetSelection.tell(msg, self());
    }

    private void aggregateAndForward() {
        System.out.println(this.config.getObject("stream-processing.partitions").unwrapped().get(keyPartition) + " partition");
        Double aggregatedValue = aggregateFunction.apply(window.getValues());
        LocalDateTime timestamp = LocalDateTime.now();
        String nextKey = keyPartition;

        synchronized (Operator.class) {
            sender().tell(new AggregatedData(nextKey,
                    aggregatedValue,
                    sequenceNumber,
                    timestamp,
                    aggregationType), self());
        }

        log.info("Aggregated value: {} for key: {}", aggregatedValue, nextKey);
    }

    private boolean verifyCorrectPartition(String partition) {
        return partition.equalsIgnoreCase(keyPartition);
    }

    // Factory method to create instances of the Operator with the appropriate aggregation function
    public static Props props(String keyPartition, Config config) {
        Function<List<Double>, Double> aggregationFunction = getAggregationFunction(config.getString("stream-processing.operators." + keyPartition + ".aggregation-function"));
        return Props.create(Operator.class, () -> new Operator(keyPartition, aggregationFunction));
    }

    @Override
    public void preRestart(Throwable reason, Optional<Object> message) throws Exception {
        super.preRestart(reason, message);
        log.info("Operator restart");
    }

    @Override
    public void postRestart(Throwable reason) throws Exception {
        super.postRestart(reason);
        log.info("Operator restarted");
    }

    private static Function<List<Double>, Double> getAggregationFunction(String functionName) {
        return switch (functionName.toLowerCase()) {
            case "average" -> values -> values.stream().mapToDouble(val -> val).average().orElse(0.0);
            case "sum" -> values -> values.stream().mapToDouble(val -> val).sum();
            case "max" -> values -> values.stream().mapToDouble(val -> val).max().orElse(0.0);
            case "min" -> values -> values.stream().mapToDouble(val -> val).min().orElse(0.0);
            default -> throw new IllegalArgumentException("Unknown aggregation function: " + functionName);
        };
    }
}
