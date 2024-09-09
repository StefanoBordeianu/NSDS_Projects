package polimi.server;

import akka.actor.ActorRef;
import akka.actor.ActorSystem;
import akka.pattern.Patterns;
import akka.util.Timeout;
import com.typesafe.config.Config;
import com.typesafe.config.ConfigFactory;
import polimi.server.actors.FileWriterActor;
import polimi.server.actors.StreamManager;
import polimi.server.messages.WatchMe;
import polimi.server.supervisors.StreamSupervisor;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import scala.concurrent.Await;
import scala.concurrent.Future;
import scala.concurrent.duration.Duration;

import java.util.Random;
import java.util.concurrent.TimeUnit;

public class StreamProcessingMain {

    private static final Logger log = LoggerFactory.getLogger(StreamProcessingMain.class);

    private static final Random ran = new Random();

    public static void main(String[] args) {

        try {
            Config config = ConfigFactory.load("server.conf");

            ActorSystem system = ActorSystem.create("StreamProcessingSystem", config);
            Timeout timeout = new Timeout(Duration.create(5, TimeUnit.SECONDS));
            ActorRef streamSupervisor = system.actorOf(StreamSupervisor.props(), "stream-supervisor");

            ActorRef writer = system.actorOf(FileWriterActor.props("aggregated_data1.json"), "stream-writer");

            // Create stream manager actor
            Future<Object> waitForManager = Patterns.ask(streamSupervisor, StreamManager.props(), timeout);
            ActorRef manager = (ActorRef) Await.result(waitForManager, timeout.duration());

            manager.tell(new WatchMe(writer), ActorRef.noSender());

        } catch (Exception e) {
            log.error("Error starting stream processing system", e);
        } finally {
            //exec.shutdown();
            //system.terminate();
        }
    }
}
