//! Bounded single-input effect graphs. Branch/merge nodes require a separate renderer contract.
pub const MAX_EFFECT_NODES: usize = 8;

#[derive(Debug, PartialEq, Eq)]
pub enum GraphError {
    ResourceLimit,
    InvalidReference,
    Cycle,
    UnreachableNode,
}

/// `-1` denotes the source image; other inputs are node indices. Returns render order.
pub fn render_order(inputs: &[i32], output: usize) -> Result<Vec<usize>, GraphError> {
    if inputs.is_empty() || inputs.len() > MAX_EFFECT_NODES {
        return Err(GraphError::ResourceLimit);
    }
    if output >= inputs.len() || inputs.iter().any(|&i| i < -1 || i >= inputs.len() as i32) {
        return Err(GraphError::InvalidReference);
    }
    let mut seen = [false; MAX_EFFECT_NODES];
    let mut order = Vec::with_capacity(inputs.len());
    let mut current = output as i32;
    while current != -1 {
        let index = current as usize;
        if seen[index] {
            return Err(GraphError::Cycle);
        }
        seen[index] = true;
        order.push(index);
        current = inputs[index];
    }
    if order.len() != inputs.len() {
        return Err(GraphError::UnreachableNode);
    }
    order.reverse();
    Ok(order)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn orders_forward_references_without_requiring_sorted_storage() {
        assert_eq!(render_order(&[2, -1, 1], 0), Ok(vec![1, 2, 0]));
    }
    #[test]
    fn rejects_cycles_invalid_references_unused_nodes_and_resource_exhaustion() {
        assert_eq!(render_order(&[1, 0], 0), Err(GraphError::Cycle));
        assert_eq!(render_order(&[-2], 0), Err(GraphError::InvalidReference));
        assert_eq!(render_order(&[9], 0), Err(GraphError::InvalidReference));
        assert_eq!(render_order(&[-1], 1), Err(GraphError::InvalidReference));
        assert_eq!(render_order(&[-1, -1], 0), Err(GraphError::UnreachableNode));
        assert_eq!(render_order(&[], 0), Err(GraphError::ResourceLimit));
        assert_eq!(render_order(&[-1; 9], 0), Err(GraphError::ResourceLimit));
    }
    #[test]
    fn permits_the_full_bounded_chain() {
        assert_eq!(
            render_order(&[-1, 0, 1, 2, 3, 4, 5, 6], 7),
            Ok((0..8).collect())
        );
    }
}
