package polimi.server.messages;

import akka.actor.ActorRef;
import lombok.Getter;

@Getter
public class WatchMe {

    private final ActorRef actorToWatch;

    public WatchMe(ActorRef actorToWatch) {
        this.actorToWatch = actorToWatch;
    }
}
