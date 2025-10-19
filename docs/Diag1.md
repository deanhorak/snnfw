graph TB
    subgraph "HIERARCHICAL ORGANIZATION"
        Brain["<b>Brain</b><br/>Top-level container<br/>stores Hemisphere IDs"]
        
        Hemisphere1["<b>Hemisphere</b><br/>Left/Right brain<br/>stores Lobe IDs"]
        Hemisphere2["<b>Hemisphere</b><br/>(Right)"]
        
        Lobe1["<b>Lobe</b><br/>Frontal, Parietal, etc.<br/>stores Region IDs"]
        Lobe2["<b>Lobe</b><br/>(Occipital)"]
        
        Region1["<b>Region</b><br/>Visual Cortex, Motor Cortex<br/>stores Nucleus IDs"]
        Region2["<b>Region</b><br/>(V1)"]
        
        Nucleus1["<b>Nucleus</b><br/>Functional groups<br/>stores Column IDs"]
        Nucleus2["<b>Nucleus</b><br/>(Layer 4C)"]
        
        Column1["<b>Column</b><br/>Cortical columns<br/>stores Layer IDs"]
        Column2["<b>Column</b><br/>(Orientation)"]
        
        Layer1["<b>Layer</b><br/>Cortical layers I-VI<br/>stores Cluster IDs"]
        Layer2["<b>Layer</b><br/>(Layer IV)"]
        
        Cluster1["<b>Cluster</b><br/>Groups of neurons<br/>stores Neuron IDs"]
        Cluster2["<b>Cluster</b><br/>(Edge detectors)"]
        
        Brain --> Hemisphere1
        Brain -.-> Hemisphere2
        Hemisphere1 --> Lobe1
        Hemisphere1 -.-> Lobe2
        Lobe1 --> Region1
        Lobe1 -.-> Region2
        Region1 --> Nucleus1
        Region1 -.-> Nucleus2
        Nucleus1 --> Column1
        Nucleus1 -.-> Column2
        Column1 --> Layer1
        Column1 -.-> Layer2
        Layer1 --> Cluster1
        Layer1 -.-> Cluster2
    end
    
    subgraph "NEURON LEVEL"
        Cluster1 --> N1["<b>Neuron 1</b><br/>Pattern learning<br/>Spike processing"]
        Cluster1 --> N2["<b>Neuron 2</b><br/>windowSize: 200ms<br/>threshold: 0.7"]
        Cluster1 --> N3["<b>Neuron 3</b><br/>maxPatterns: 100"]
    end
    
    subgraph "CONNECTIVITY - Neuron to Neuron"
        direction LR
        
        SrcNeuron["<b>Source Neuron</b><br/>ID: 12345<br/>axonId: 67890"]
        
        Axon1["<b>Axon</b><br/>ID: 67890<br/>sourceNeuronId: 12345<br/>synapseIds: [111, 112, 113]"]
        
        Syn1["<b>Synapse 1</b><br/>ID: 111<br/>weight: 0.8<br/>dendriteId: 201"]
        Syn2["<b>Synapse 2</b><br/>ID: 112<br/>weight: 0.6<br/>dendriteId: 202"]
        Syn3["<b>Synapse 3</b><br/>ID: 113<br/>weight: 0.9<br/>dendriteId: 203"]
        
        Dend1["<b>Dendrite 1</b><br/>ID: 201<br/>targetNeuronId: 54321"]
        Dend2["<b>Dendrite 2</b><br/>ID: 202<br/>targetNeuronId: 54322"]
        Dend3["<b>Dendrite 3</b><br/>ID: 203<br/>targetNeuronId: 54323"]
        
        TgtNeuron1["<b>Target Neuron 1</b><br/>ID: 54321<br/>dendriteIds: [201, ...]"]
        TgtNeuron2["<b>Target Neuron 2</b><br/>ID: 54322<br/>dendriteIds: [202, ...]"]
        TgtNeuron3["<b>Target Neuron 3</b><br/>ID: 54323<br/>dendriteIds: [203, ...]"]
        
        SrcNeuron -->|"has one<br/>axonId"| Axon1
        Axon1 -->|"has many<br/>synapseIds"| Syn1
        Axon1 --> Syn2
        Axon1 --> Syn3
        Syn1 -->|"connects to<br/>dendriteId"| Dend1
        Syn2 --> Dend2
        Syn3 --> Dend3
        Dend1 -->|"belongs to<br/>targetNeuronId"| TgtNeuron1
        Dend2 --> TgtNeuron2
        Dend3 --> TgtNeuron3
    end
    
    style Brain fill:#ff9999,stroke:#cc0000,stroke-width:3px
    style Hemisphere1 fill:#ffcc99,stroke:#cc6600,stroke-width:2px
    style Lobe1 fill:#ffff99,stroke:#cccc00,stroke-width:2px
    style Region1 fill:#ccff99,stroke:#66cc00,stroke-width:2px
    style Nucleus1 fill:#99ffcc,stroke:#00cc66,stroke-width:2px
    style Column1 fill:#99ccff,stroke:#0066cc,stroke-width:2px
    style Layer1 fill:#cc99ff,stroke:#6600cc,stroke-width:2px
    style Cluster1 fill:#ff99ff,stroke:#cc00cc,stroke-width:2px
    
    style N1 fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    style N2 fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    style N3 fill:#ffcccc,stroke:#ff0000,stroke-width:2px
    
    style SrcNeuron fill:#ffcccc,stroke:#ff0000,stroke-width:3px
    style Axon1 fill:#ccffcc,stroke:#00cc00,stroke-width:2px
    style Syn1 fill:#ffffcc,stroke:#cccc00,stroke-width:2px
    style Syn2 fill:#ffffcc,stroke:#cccc00,stroke-width:2px
    style Syn3 fill:#ffffcc,stroke:#cccc00,stroke-width:2px
    style Dend1 fill:#ccccff,stroke:#0000cc,stroke-width:2px
    style Dend2 fill:#ccccff,stroke:#0000cc,stroke-width:2px
    style Dend3 fill:#ccccff,stroke:#0000cc,stroke-width:2px
    style TgtNeuron1 fill:#ffcccc,stroke:#ff0000,stroke-width:3px
    style TgtNeuron2 fill:#ffcccc,stroke:#ff0000,stroke-width:3px
    style TgtNeuron3 fill:#ffcccc,stroke:#ff0000,stroke-width:3px
