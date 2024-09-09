package polimi.client;

import akka.actor.AbstractActor;
import akka.actor.ActorSelection;
import akka.actor.Props;
import polimi.server.messages.SensorData;

public class SenderActor extends AbstractActor {

    private final String SERVER_ADDRESS = "akka://StreamProcessingSystem@127.0.0.1:8080/user/stream-supervisor/stream-manager";

    private final ActorSelection server;

    public SenderActor(){
        server = context().actorSelection(SERVER_ADDRESS);
        System.out.println("SenderActor created");
    }

    @Override
    public Receive createReceive() {
        return receiveBuilder()
                .match(SensorData.class, msg -> server.tell(msg, self()))
                .build();
    }

    public static Props props(){return Props.create(SenderActor.class);}
}
