package polimi.client;

import akka.actor.ActorRef;
import akka.actor.ActorSystem;
import com.typesafe.config.Config;
import com.typesafe.config.ConfigFactory;
import polimi.server.messages.SensorData;

import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class Client {

    private static final int numThreads = 10;

    public static final int NORM_OP = 0;
    public static final int FAULT_OPERATOR_OP = 3;

    public static void main(String[] args) {

        // Send messages from multiple threads in parallel
        final ExecutorService exec = Executors.newFixedThreadPool(numThreads);

        try {
            Config config = ConfigFactory.load("client.conf");

            ActorSystem sys = ActorSystem.create("ClientSystem", config);

            ActorRef clientActor = sys.actorOf(SenderActor.props(), "ClientActor");

            Random ran = new Random();

            while (true) {
                for(int i = 0; i < ran.nextInt(1, 500); i++){
                    if (ran.nextInt(0, 100) < 98){
                        exec.submit(() -> clientActor.tell(new SensorData("temperature", ran.nextDouble() * 100), ActorRef.noSender()));
                        exec.submit(() -> clientActor.tell(new SensorData("humidity", ran.nextDouble() * 100), ActorRef.noSender()));
                        exec.submit(() -> clientActor.tell(new SensorData("pressure", ran.nextDouble() * 100), ActorRef.noSender()));
                    } else {
                        exec.submit(() -> clientActor.tell(new SensorData("temperature", ran.nextDouble() * 100, FAULT_OPERATOR_OP), ActorRef.noSender()));
                        exec.submit(() -> clientActor.tell(new SensorData("humidity", ran.nextDouble() * 100, FAULT_OPERATOR_OP), ActorRef.noSender()));
                        exec.submit(() -> clientActor.tell(new SensorData("pressure", ran.nextDouble() * 100, FAULT_OPERATOR_OP), ActorRef.noSender()));
                    }

                }
                Thread.sleep(3000);
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            exec.shutdown();
        }
    }
}
