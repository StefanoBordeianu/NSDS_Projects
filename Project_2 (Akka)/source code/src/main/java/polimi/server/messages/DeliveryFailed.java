package polimi.server.messages;

import lombok.Getter;

@Getter
public class DeliveryFailed {

    private final Object message;

    public DeliveryFailed(Object message) {
        this.message = message;
    }
}
