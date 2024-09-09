package polimi.server.utils;

import lombok.Getter;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@Getter
public class Window {

    private final int size;
    private final int slide;
    private final double[] elements;
    private int count; // Number of elements currently in the window

    public Window(int size, int slide) {
        if (slide > size) {
            throw new IllegalArgumentException("Slide value cannot be greater than window size");
        }
        this.size = size;
        this.slide = slide;
        this.elements = new double[size];
        this.count = 0; // Initialize count to 0
    }

    /**
     * Inserts a new value into the window.
     * If the window is not full, the value is added at the end.
     * If the window is full, the oldest element is overwritten.
     *
     * @param value The value to be inserted.
     */
    public void insert(double value) {
        if (count < size) {
            elements[count] = value;
            count++;
        } else {
            // If the window is full, slide and then insert the new value at the end
            slide(slide);
            elements[count - slide] = value;
            count = size; // Ensure the window stays full after sliding
        }
    }

    /**
     * Slides the window by the specified number of positions.
     * Shifts elements to the left and fills the end with zeroes.
     *
     * @param slidePositions Number of positions to slide.
     */
    public void slide(int slidePositions) {
        if (slidePositions > size) {
            throw new IllegalArgumentException("Slide value cannot be greater than window size: " + size);
        }

        // Shift elements to the left by slide positions
        System.arraycopy(elements, slidePositions, elements, 0, size - slidePositions);

        // Fill the remaining slots with 0.0 (or any default value)
        Arrays.fill(elements, size - slidePositions, size, 0.0);

        // Adjust count to reflect the new number of valid elements in the window
        count -= slidePositions;
        if (count < 0) {
            count = 0;
        }
    }

    /**
     * Checks if the window is full.
     *
     * @return True if the window is full, otherwise false.
     */
    public boolean isFull() {
        return count == size;
    }

    /**
     * Returns the current values in the window as a list.
     *
     * @return List of current values in the window.
     */
    public List<Double> getValues() {
        List<Double> values = new ArrayList<>();
        for (int i = 0; i < count; i++) {
            values.add(elements[i]);
        }
        return values;
    }
}
