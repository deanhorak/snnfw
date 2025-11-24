graph LR
    subgraph "SOURCE NEURONS"
        N1["<b>Neuron 1</b><br/>ID: 100<br/>axonId: 1000<br/>dendriteIds: []"]
        N2["<b>Neuron 2</b><br/>ID: 101<br/>axonId: 1001<br/>dendriteIds: []"]
        N3["<b>Neuron 3</b><br/>ID: 102<br/>axonId: 1002<br/>dendriteIds: []"]
    end
    
    subgraph "AXONS - Output Terminals"
        A1["<b>Axon 1</b><br/>ID: 1000<br/>sourceNeuronId: 100<br/>synapseIds: [2000, 2001]"]
        A2["<b>Axon 2</b><br/>ID: 1001<br/>sourceNeuronId: 101<br/>synapseIds: [2002, 2003]"]
        A3["<b>Axon 3</b><br/>ID: 1002<br/>sourceNeuronId: 102<br/>synapseIds: [2004]"]
    end
    
    subgraph "SYNAPSES - Connections"
        S1["<b>Synapse 1</b><br/>ID: 2000<br/>weight: 0.8<br/>dendriteId: 3000"]
        S2["<b>Synapse 2</b><br/>ID: 2001<br/>weight: 0.6<br/>dendriteId: 3001"]
        S3["<b>Synapse 3</b><br/>ID: 2002<br/>weight: 0.9<br/>dendriteId: 3000"]
        S4["<b>Synapse 4</b><br/>ID: 2003<br/>weight: 0.7<br/>dendriteId: 3002"]
        S5["<b>Synapse 5</b><br/>ID: 2004<br/>weight: 0.5<br/>dendriteId: 3001"]
    end
    
    subgraph "DENDRITES - Input Terminals"
        D1["<b>Dendrite 1</b><br/>ID: 3000<br/>targetNeuronId: 200"]
        D2["<b>Dendrite 2</b><br/>ID: 3001<br/>targetNeuronId: 201"]
        D3["<b>Dendrite 3</b><br/>ID: 3002<br/>targetNeuronId: 202"]
    end
    
    subgraph "TARGET NEURONS"
        T1["<b>Neuron 4</b><br/>ID: 200<br/>axonId: 0<br/>dendriteIds: [3000]<br/><br/>Receives from:<br/>Neuron 1 (w=0.8)<br/>Neuron 2 (w=0.9)"]
        T2["<b>Neuron 5</b><br/>ID: 201<br/>axonId: 0<br/>dendriteIds: [3001]<br/><br/>Receives from:<br/>Neuron 1 (w=0.6)<br/>Neuron 3 (w=0.5)"]
        T3["<b>Neuron 6</b><br/>ID: 202<br/>axonId: 0<br/>dendriteIds: [3002]<br/><br/>Receives from:<br/>Neuron 2 (w=0.7)"]
    end
    
    N1 -->|"has axonId"| A1
    N2 -->|"has axonId"| A2
    N3 -->|"has axonId"| A3
    
    A1 -->|"synapseIds[0]"| S1
    A1 -->|"synapseIds[1]"| S2
    A2 -->|"synapseIds[0]"| S3
    A2 -->|"synapseIds[1]"| S4
    A3 -->|"synapseIds[0]"| S5
    
    S1 -->|"dendriteId"| D1
    S2 -->|"dendriteId"| D2
    S3 -->|"dendriteId"| D1
    S4 -->|"dendriteId"| D3
    S5 -->|"dendriteId"| D2
    
    D1 -->|"targetNeuronId"| T1
    D2 -->|"targetNeuronId"| T2
    D3 -->|"targetNeuronId"| T3
    
    T1 -.->|"dendriteIds[0]"| D1
    T2 -.->|"dendriteIds[0]"| D2
    T3 -.->|"dendriteIds[0]"| D3
    
    style N1 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
    style N2 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
    style N3 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
    
    style A1 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style A2 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    style A3 fill:#c8e6c9,stroke:#388e3c,stroke-width:2px
    
    style S1 fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    style S2 fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    style S3 fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    style S4 fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    style S5 fill:#fff9c4,stroke:#f57f17,stroke-width:2px
    
    style D1 fill:#bbdefb,stroke:#1565c0,stroke-width:2px
    style D2 fill:#bbdefb,stroke:#1565c0,stroke-width:2px
    style D3 fill:#bbdefb,stroke:#1565c0,stroke-width:2px
    
    style T1 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
    style T2 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
    style T3 fill:#ffcdd2,stroke:#c62828,stroke-width:3px
